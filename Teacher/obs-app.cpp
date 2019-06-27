
#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <chrono>
#include <ratio>
#include <string>
#include <sstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <obs-config.h>
#include <obs.hpp>

#include <QGuiApplication>
#include <QProxyStyle>
#include <QScreen>

#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
//#include "window-basic-settings.hpp"
//#include "window-license-agreement.hpp"
#include "crash-report.hpp"
#include "platform.hpp"
#include "FastSession.h"
#include "getopt.h"

#include <fstream>
#include <jansson.h>
#include <curl/curl.h>

#include <QtCore/qfile.h>
#include <QNetworkRequest>
#include <QNetworkReply>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include <iostream>
#include <IPTypes.h>
#include <IPHlpApi.h>
#include <atlconv.h>

#pragma comment(lib, "iphlpapi.lib")

using namespace std;
static log_handler_t def_log_handler;

static string currentLogFile;
static string lastLogFile;
static string lastCrashLogFile;

bool portable_mode = false;
static bool multi = false;
static bool log_verbose = false;
static bool unfiltered_log = false;
bool opt_start_streaming = false;
bool opt_start_recording = false;
bool opt_studio_mode = false;
bool opt_start_replaybuffer = false;
bool opt_minimize_tray = false;
bool opt_allow_opengl = false;
bool opt_always_on_top = false;
string opt_starting_collection;
string opt_starting_profile;
string opt_starting_scene;

// AMD PowerXpress High Performance Flags
#ifdef _MSC_VER
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

QObject *CreateShortcutFilter()
{
	return new OBSEventFilter([](QObject *obj, QEvent *event)
	{
		auto mouse_event = [](QMouseEvent &event)
		{
			obs_key_combination_t hotkey = { 0, OBS_KEY_NONE };
			bool pressed = event.type() == QEvent::MouseButtonPress;

			switch (event.button()) {
			case Qt::NoButton:
			case Qt::LeftButton:
			case Qt::RightButton:
			case Qt::AllButtons:
			case Qt::MouseButtonMask:
				return false;

			case Qt::MidButton:
				hotkey.key = OBS_KEY_MOUSE3;
				break;

#define MAP_BUTTON(i, j) case Qt::ExtraButton ## i: \
		hotkey.key = OBS_KEY_MOUSE ## j; break;
				MAP_BUTTON(1, 4);
				MAP_BUTTON(2, 5);
				MAP_BUTTON(3, 6);
				MAP_BUTTON(4, 7);
				MAP_BUTTON(5, 8);
				MAP_BUTTON(6, 9);
				MAP_BUTTON(7, 10);
				MAP_BUTTON(8, 11);
				MAP_BUTTON(9, 12);
				MAP_BUTTON(10, 13);
				MAP_BUTTON(11, 14);
				MAP_BUTTON(12, 15);
				MAP_BUTTON(13, 16);
				MAP_BUTTON(14, 17);
				MAP_BUTTON(15, 18);
				MAP_BUTTON(16, 19);
				MAP_BUTTON(17, 20);
				MAP_BUTTON(18, 21);
				MAP_BUTTON(19, 22);
				MAP_BUTTON(20, 23);
				MAP_BUTTON(21, 24);
				MAP_BUTTON(22, 25);
				MAP_BUTTON(23, 26);
				MAP_BUTTON(24, 27);
#undef MAP_BUTTON
			}

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event.modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		auto key_event = [&](QKeyEvent *event)
		{
			QDialog *dialog = qobject_cast<QDialog*>(obj);

			obs_key_combination_t hotkey = { 0, OBS_KEY_NONE };
			bool pressed = event->type() == QEvent::KeyPress;

			switch (event->key()) {
			case Qt::Key_Shift:
			case Qt::Key_Control:
			case Qt::Key_Alt:
			case Qt::Key_Meta:
				break;

#ifdef __APPLE__
			case Qt::Key_CapsLock:
				// kVK_CapsLock == 57
				hotkey.key = obs_key_from_virtual_key(57);
				pressed = true;
				break;
#endif

			case Qt::Key_Enter:
			case Qt::Key_Escape:
			case Qt::Key_Return:
				if (dialog && pressed)
					return false;
			default:
				hotkey.key = obs_key_from_virtual_key(
					event->nativeVirtualKey());
			}

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event->modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
			return mouse_event(*static_cast<QMouseEvent*>(event));

		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return key_event(static_cast<QKeyEvent*>(event));

		default:
			return false;
		}
	});
}

string CurrentTimeString()
{
	using namespace std::chrono;

	struct tm  tstruct;
	char       buf[80];

	auto tp = system_clock::now();
	auto now = system_clock::to_time_t(tp);
	tstruct = *localtime(&now);

	size_t written = strftime(buf, sizeof(buf), "%X", &tstruct);
	if (ratio_less<system_clock::period, seconds::period>::value &&
		written && (sizeof(buf) - written) > 5) {
		auto tp_secs =
			time_point_cast<seconds>(tp);
		auto millis =
			duration_cast<milliseconds>(tp - tp_secs).count();

		snprintf(buf + written, sizeof(buf) - written, ".%03u",
			static_cast<unsigned>(millis));
	}

	return buf;
}

string CurrentDateTimeString()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d, %X", &tstruct);
	return buf;
}

static inline void LogString(fstream &logFile, const char *timeString,
	char *str)
{
	logFile << timeString << str << endl;
}

static inline void LogStringChunk(fstream &logFile, char *str)
{
	char *nextLine = str;
	string timeString = CurrentTimeString();
	timeString += ": ";

	while (*nextLine) {
		char *nextLine = strchr(str, '\n');
		if (!nextLine)
			break;

		if (nextLine != str && nextLine[-1] == '\r') {
			nextLine[-1] = 0;
		}
		else {
			nextLine[0] = 0;
		}

		LogString(logFile, timeString.c_str(), str);
		nextLine++;
		str = nextLine;
	}

	LogString(logFile, timeString.c_str(), str);
}

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

static inline int sum_chars(const char *str)
{
	int val = 0;
	for (; *str != 0; str++)
		val += *str;

	return val;
}

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,
	const char *output_str)
{
	static mutex log_mutex;
	static const char *last_msg_ptr = nullptr;
	static int last_char_sum = 0;
	static char cmp_str[4096];
	static int rep_count = 0;

	int new_sum = sum_chars(output_str);

	lock_guard<mutex> guard(log_mutex);

	if (unfiltered_log) {
		return false;
	}

	if (last_msg_ptr == msg) {
		int diff = std::abs(new_sum - last_char_sum);
		if (diff < MAX_CHAR_VARIATION) {
			return (rep_count++ >= MAX_REPEATED_LINES);
		}
	}

	if (rep_count > MAX_REPEATED_LINES) {
		logFile << CurrentTimeString() <<
			": Last log entry repeated for " <<
			to_string(rep_count - MAX_REPEATED_LINES) <<
			" more lines" << endl;
	}

	last_msg_ptr = msg;
	strcpy(cmp_str, output_str);
	last_char_sum = new_sum;
	rep_count = 0;

	return false;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	fstream &logFile = *static_cast<fstream*>(param);
	char str[4096];

#ifndef _WIN32
	va_list args2;
	va_copy(args2, args);
#endif

	vsnprintf(str, 4095, msg, args);

#ifdef _WIN32
	if (IsDebuggerPresent()) {
		// 调试信息新增时间戳...
		string strTemp = str;
		char szBuf[32] = { 0 };
		sprintf(szBuf, "[%I64d ms]", os_gettime_ns() / 1000000);
		sprintf(str, "%s %s", szBuf, strTemp.c_str());
		// 进行宽字符格式转换...
		int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
		if (wNum > 1) {
			static wstring wide_buf;
			static mutex wide_mutex;

			lock_guard<mutex> lock(wide_mutex);
			wide_buf.reserve(wNum + 1);
			wide_buf.resize(wNum - 1);
			MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0],
				wNum);
			wide_buf.push_back('\n');

			OutputDebugStringW(wide_buf.c_str());
		}
	}
#else
	def_log_handler(log_level, msg, args2, nullptr);
#endif

	if (log_level <= LOG_INFO || log_verbose) {
		if (too_many_repeated_entries(logFile, msg, str))
			return;
		LogStringChunk(logFile, str);
	}

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif
}

// 修改默认语言 => zh-CN
#define DEFAULT_LANG "zh-CN"
//#define DEFAULT_LANG "en-US"

bool OBSApp::InitGlobalConfigDefaults()
{
	config_set_default_string(globalConfig, "General", "Language", DEFAULT_LANG);
	config_set_default_uint(globalConfig, "General", "MaxLogs", 10);
	config_set_default_string(globalConfig, "General", "ProcessPriority", "Normal");
	config_set_default_bool(globalConfig, "General", "EnableAutoUpdates", true);

#if _WIN32
	config_set_default_string(globalConfig, "Video", "Renderer", "Direct3D 11");
#else
	config_set_default_string(globalConfig, "Video", "Renderer", "OpenGL");
#endif

	config_set_default_bool(globalConfig, "BasicWindow", "PreviewEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "PreviewProgramMode", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SceneDuplicationMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SwapScenesMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SnappingEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ScreenSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SourceSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow", "CenterSnapping", false);
	config_set_default_double(globalConfig, "BasicWindow", "SnapDistance", 10.0);
	config_set_default_bool(globalConfig, "BasicWindow", "RecordWhenStreaming", false);
	config_set_default_bool(globalConfig, "BasicWindow", "KeepRecordingWhenStreamStops", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayWhenStarted", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SaveProjectors", false);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowTransitions", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowListboxToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowStatusBar", true);

	if (!config_get_bool(globalConfig, "General", "Pre21Defaults")) {
		config_set_default_string(globalConfig, "General", "CurrentTheme", "Default");
	}

#ifdef _WIN32
	config_set_default_bool(globalConfig, "Audio", "DisableAudioDucking", true);
#endif

#ifdef __APPLE__
	config_set_default_bool(globalConfig, "Video", "DisableOSXVSync", true);
	config_set_default_bool(globalConfig, "Video", "ResetOSXVSyncOnExit", true);
#endif
	return true;
}

static bool do_mkdir(const char *path)
{
	if (os_mkdirs(path) == MKDIR_ERROR) {
		OBSErrorBox(NULL, "Failed to create directory %s", path);
		return false;
	}

	return true;
}

static bool MakeUserDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-teacher/basic") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-teacher/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-teacher/profiler_data") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	if (GetConfigPath(path, sizeof(path), "obs-teacher/crashes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-teacher/updates") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
#endif

	if (GetConfigPath(path, sizeof(path), "obs-teacher/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static bool MakeUserProfileDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-teacher/basic/profiles") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-teacher/basic/scenes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static string GetProfileDirFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-teacher/basic/profiles") <= 0)
		return outputPath;

	strcat(path, "/*");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (!ent.directory)
			continue;

		strcpy(path, ent.path);
		strcat(path, "/basic.ini");

		ConfigFile config;
		if (config.Open(path, CONFIG_OPEN_EXISTING) != 0)
			continue;

		const char *curName = config_get_string(config, "General",
			"Name");
		if (astrcmpi(curName, name) == 0) {
			outputPath = ent.path;
			break;
		}
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

static string GetSceneCollectionFileFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-teacher/basic/scenes") <= 0)
		return outputPath;

	strcat(path, "/*.json");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (ent.directory)
			continue;

		obs_data_t *data =
			obs_data_create_from_json_file_safe(ent.path, "bak");
		const char *curName = obs_data_get_string(data, "name");

		if (astrcmpi(name, curName) == 0) {
			outputPath = ent.path;
			obs_data_release(data);
			break;
		}

		obs_data_release(data);
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		outputPath.resize(outputPath.size() - 5);
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}
// 注意：统一返回UTF8格式...
char * OBSApp::GetServerDNSName()
{
	static char szBuffer[MAX_PATH] = { 0 };
	if (strlen(szBuffer) > 0)
		return szBuffer;
	DWORD dwLen = MAX_PATH;
	static WCHAR szTempName[MAX_PATH] = { 0 };
	::GetComputerName(szTempName, &dwLen);
	os_wcs_to_utf8(szTempName, MAX_PATH, szBuffer, MAX_PATH);
	return szBuffer;
}

bool OBSApp::InitGlobalConfig()
{
	char path[512];
	bool changed = false;

	int len = GetConfigPath(path, sizeof(path),	"obs-teacher/global.ini");
	if (len <= 0) {
		return false;
	}

	int errorcode = globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}

	if (!opt_starting_collection.empty()) {
		string path = GetSceneCollectionFileFromName(opt_starting_collection.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "SceneCollection", opt_starting_collection.c_str());
			config_set_string(globalConfig, "Basic", "SceneCollectionFile", path.c_str());
			changed = true;
		}
	}

	if (!opt_starting_profile.empty()) {
		string path = GetProfileDirFromName(opt_starting_profile.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "Profile",	opt_starting_profile.c_str());
			config_set_string(globalConfig, "Basic", "ProfileDir", path.c_str());
			changed = true;
		}
	}

	if (!config_has_user_value(globalConfig, "General", "Pre19Defaults")) {
		//uint32_t lastVersion = config_get_int(globalConfig, "General", "LastVersion");
		//bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(19, 0, 0);
		// 不要使用老版本的默认设置，直接设定为false状态...
		config_set_bool(globalConfig, "General", "Pre19Defaults", false);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre21Defaults")) {
		//uint32_t lastVersion = config_get_int(globalConfig, "General", "LastVersion");
		//bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(21, 0, 0);
		// 不要使用老版本的默认设置，直接设定为false状态...
		config_set_bool(globalConfig, "General", "Pre21Defaults", false);
		changed = true;
	}

	// 配置文件中没有网络配置参数，设置硬编码的网络配置参数...
	if (!config_has_user_value(globalConfig, "General", "WebCenter")) {
		config_set_string(globalConfig, "General", "WebCenter", DEF_WEB_CENTER);
		changed = true;
	}
	// 从配置文件当中读取中心网站的地址...
	m_strWebCenter = config_get_string(globalConfig, "General", "WebCenter");
	// 查看节点网站地址是否有效...
	if (!config_has_user_value(globalConfig, "General", "WebClass")) {
		config_set_string(globalConfig, "General", "WebClass", DEF_WEB_CLASS);
		changed = true;
	}
	// 从配置文件当中读取节点网站的地址...
	m_strWebClass = config_get_string(globalConfig, "General", "WebClass");
	// 配置有变化，存盘到global.ini配置文件当中...
	if (changed) {
		config_save_safe(globalConfig, "tmp", nullptr);
	}
	// 返回默认的初始配置信息...
	return InitGlobalConfigDefaults();
}

bool OBSApp::InitLocale()
{
	ProfileScope("OBSApp::InitLocale");
	const char *lang = config_get_string(globalConfig, "General",
		"Language");

	locale = lang;

	string englishPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", englishPath)) {
		OBSErrorBox(NULL, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}

	textLookup = text_lookup_create(englishPath.c_str());
	if (!textLookup) {
		OBSErrorBox(NULL, "Failed to create locale from file '%s'",
			englishPath.c_str());
		return false;
	}

	bool userLocale = config_has_user_value(globalConfig, "General",
		"Language");
	bool defaultLang = astrcmpi(lang, DEFAULT_LANG) == 0;

	if (userLocale && defaultLang)
		return true;

	if (!userLocale && defaultLang) {
		for (auto &locale_ : GetPreferredLocales()) {
			if (locale_ == lang)
				return true;

			stringstream file;
			file << "locale/" << locale_ << ".ini";

			string path;
			if (!GetDataFilePath(file.str().c_str(), path))
				continue;

			if (!text_lookup_add(textLookup, path.c_str()))
				continue;

			blog(LOG_INFO, "Using preferred locale '%s'",
				locale_.c_str());
			locale = locale_;
			return true;
		}

		return true;
	}

	stringstream file;
	file << "locale/" << lang << ".ini";

	string path;
	if (GetDataFilePath(file.str().c_str(), path)) {
		if (!text_lookup_add(textLookup, path.c_str()))
			blog(LOG_ERROR, "Failed to add locale file '%s'",
				path.c_str());
	}
	else {
		blog(LOG_ERROR, "Could not find locale file '%s'",
			file.str().c_str());
	}

	return true;
}

bool OBSApp::SetTheme(std::string name, std::string path)
{
	theme = name;

	/* Check user dir first, then preinstalled themes. */
	if (path == "") {
		char userDir[512];
		name = "themes/" + name + ".qss";
		string temp = "obs-teacher/" + name;
		int ret = GetConfigPath(userDir, sizeof(userDir),
			temp.c_str());

		if (ret > 0 && QFile::exists(userDir)) {
			path = string(userDir);
		}
		else if (!GetDataFilePath(name.c_str(), path)) {
			OBSErrorBox(NULL, "Failed to find %s.", name.c_str());
			return false;
		}
	}

	QString mpath = QString("file:///") + path.c_str();
	setStyleSheet(mpath);
	return true;
}

bool OBSApp::InitTheme()
{
	const char *themeName = config_get_string(globalConfig, "General",
		"CurrentTheme");
	if (!themeName) {
		/* Use deprecated "Theme" value if available */
		themeName = config_get_string(globalConfig,
			"General", "Theme");
		if (!themeName)
			themeName = "Default";
	}

	if (strcmp(themeName, "Default") != 0 && SetTheme(themeName))
		return true;

	return SetTheme("Default");
}

// 注意：统一返回UTF8格式...
string OBSApp::getJsonString(Json::Value & inValue)
{
	string strReturn;
	char szBuffer[32] = { 0 };
	if (inValue.isInt()) {
		sprintf(szBuffer, "%d", inValue.asInt());
		strReturn = szBuffer;
	}
	else if (inValue.isString()) {
		strReturn = inValue.asString();
	}
	return strReturn;
}

// 调用位置，详见 run_program() 函数，只调用一次...
void OBSApp::doProcessCmdLine(int argc, char * argv[])
{
	int	ch = 0;
	while ((ch = getopt(argc, argv, "?hvdrz")) != EOF)
	{
		switch (ch) {
		case 'd': m_bIsDebugMode = true;  continue;
		case 'r': m_bIsDebugMode = false; continue;
		case 'z': m_strThirdURI = argv[optind++]; continue;
		case '?':
		case 'h':
		case 'v':
			blog(LOG_INFO, "-d: Run as Debug Mode => mount on Debug udpserver.");
			blog(LOG_INFO, "-r: Run as Release Mode => mount on Release udpserver.");
			break;
		}
	}
}

OBSApp::OBSApp(int &argc, char **argv, profiler_name_store_t *store)
  : QApplication(argc, argv),
	profilerNameStore(store),
	m_bIsDebugMode(false)
{
	m_strWebCenter = DEF_WEB_CENTER;
	m_strWebClass = DEF_WEB_CLASS;
	m_nRtpTCPSockFD = 0;
	m_nOnLineTimer = -1;
	m_nFastTimer = -1;
	m_nFlowTimer = -1;
	m_nDBUserID = 0;
	m_nDBFlowID = 0;
	m_LoginMini = NULL;
	m_RemoteSession = NULL;
	m_TrackerSession = NULL;
	m_StorageSession = NULL;
	sleepInhibitor = os_inhibit_sleep_create("OBS Video/audio");
}

OBSApp::~OBSApp()
{
#ifdef _WIN32
	bool disableAudioDucking = config_get_bool(globalConfig, "Audio",
		"DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(false);
#endif

#ifdef __APPLE__
	bool vsyncDiabled = config_get_bool(globalConfig, "Video",
		"DisableOSXVSync");
	bool resetVSync = config_get_bool(globalConfig, "Video",
		"ResetOSXVSyncOnExit");
	if (vsyncDiabled && resetVSync)
		EnableOSXVSync(true);
#endif

	os_inhibit_sleep_set_active(sleepInhibitor, false);
	os_inhibit_sleep_destroy(sleepInhibitor);
}

static void move_basic_to_profiles(void)
{
	char path[512];
	char new_path[512];
	os_glob_t *glob;

	/* if not first time use */
	if (GetConfigPath(path, 512, "obs-teacher/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	/* if the profiles directory doesn't already exist */
	if (GetConfigPath(new_path, 512, "obs-teacher/basic/profiles") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/*.*");
	if (os_glob(path, 0, &glob) != 0)
		return;

	strcpy(path, new_path);

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		char *file;

		if (ent.directory)
			continue;

		file = strrchr(ent.path, '/');
		if (!file++)
			continue;

		if (astrcmpi(file, "scenes.json") == 0)
			continue;

		strcpy(new_path, path);
		strcat(new_path, "/");
		strcat(new_path, file);
		os_rename(ent.path, new_path);
	}

	os_globfree(glob);
}

static void move_basic_to_scene_collections(void)
{
	char path[512];
	char new_path[512];

	if (GetConfigPath(path, 512, "obs-teacher/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	if (GetConfigPath(new_path, 512, "obs-teacher/basic/scenes") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/scenes.json");
	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	strcat(new_path, ".json");

	os_rename(path, new_path);
}

bool OBSApp::InitMacIPAddr()
{
	// 2018.01.11 - 解决 ERROR_BUFFER_OVERFLOW 的问题...
	DWORD outBuflen = sizeof(IP_ADAPTER_INFO);
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO;
	DWORD retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	// 发现缓冲区不够用，重新分配空间...
	if (retStatus == ERROR_BUFFER_OVERFLOW) {
		delete lpAdapter; lpAdapter = NULL;
		lpAdapter = (PIP_ADAPTER_INFO)new BYTE[outBuflen];
		retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	}
	// 还是发生了错误，打印错误，直接返回...
	if (retStatus != ERROR_SUCCESS) {
		blog(LOG_INFO, "GetAdaptersInfo Error: %u", retStatus);
		delete lpAdapter; lpAdapter = NULL;
		return false;
	}
	// 开始循环遍历网卡节点...
	IP_ADAPTER_INFO * lpInfo = lpAdapter;
	while (lpInfo != NULL) {
		// 打印网卡描述信息 => 2018.03.27 - 解决无线网卡的问题...
		blog(LOG_INFO, "== Adapter Type: %lu, Desc: %s ==\n", lpInfo->Type, lpInfo->Description);
		// 必须是以太网卡，必须是IPV4的网卡，必须是有效的网卡 => 71 是无线网卡...
		if ((lpInfo->Type != MIB_IF_TYPE_ETHERNET && lpInfo->Type != 71) || lpInfo->AddressLength != 6 || strcmp(lpInfo->IpAddressList.IpAddress.String, "0.0.0.0") == 0) {
			lpInfo = lpInfo->Next;
			continue;
		}
		// 获取MAC地址和IP地址，地址是相互关联的...
		char szBuffer[MAX_PATH] = { 0 };
		sprintf(szBuffer, "%02X-%02X-%02X-%02X-%02X-%02X", lpInfo->Address[0], lpInfo->Address[1], lpInfo->Address[2], lpInfo->Address[3], lpInfo->Address[4], lpInfo->Address[5]);
		m_strIPAddr = lpInfo->IpAddressList.IpAddress.String;
		m_strMacAddr = szBuffer;
		lpInfo = lpInfo->Next;
	}
	// 释放分配的缓冲区...
	delete lpAdapter;
	lpAdapter = NULL;
	return true;
}

void OBSApp::AppInit()
{
	ProfileScope("OBSApp::AppInit");

	if (!this->InitMacIPAddr())
		throw "Failed to get MAC or IP address";
	if (!InitApplicationBundle())
		throw "Failed to initialize application bundle";
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";
	if (!InitTheme())
		throw "Failed to load theme";

	config_set_default_string(globalConfig, "Basic", "Profile",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "ProfileDir",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollection",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollectionFile",
		Str("Untitled"));

#ifdef _WIN32
	bool disableAudioDucking = config_get_bool(globalConfig, "Audio",
		"DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(true);
#endif

#ifdef __APPLE__
	if (config_get_bool(globalConfig, "Video", "DisableOSXVSync"))
		EnableOSXVSync(false);
#endif

	move_basic_to_profiles();
	move_basic_to_scene_collections();

	if (!MakeUserProfileDirs())
		throw "Failed to create profile directories";
}

const char *OBSApp::GetRenderModule() const
{
	const char *renderer = config_get_string(globalConfig, "Video",
		"Renderer");

	return (astrcmpi(renderer, "Direct3D 11") == 0) ?
		DL_D3D11 : DL_OPENGL;
}

static bool StartupOBS(const char *locale, profiler_name_store_t *store)
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-teacher/plugin_config") <= 0)
		return false;

	return obs_startup(locale, path, store);
}

bool OBSApp::OBSInit()
{
	ProfileScope("OBSApp::OBSInit");

	bool licenseAccepted = config_get_bool(globalConfig, "General", "LicenseAccepted");
	//OBSLicenseAgreement agreement(nullptr);

	//if (licenseAccepted || agreement.exec() == QDialog::Accepted) {
	if ( true ) {
		if (!licenseAccepted) {
			config_set_bool(globalConfig, "General", "LicenseAccepted", true);
			config_save(globalConfig);
		}

		if (!StartupOBS(locale.c_str(), GetProfilerNameStore()))
			return false;

		blog(LOG_INFO, "Portable mode: %s", portable_mode ? "true" : "false");

		setQuitOnLastWindowClosed(false);

		mainWindow = new OBSBasic();

		mainWindow->setAttribute(Qt::WA_DeleteOnClose, true);
		connect(mainWindow, SIGNAL(destroyed()), this, SLOT(quit()));

		mainWindow->OBSInit();

		connect(this, &QGuiApplication::applicationStateChanged,
			[](Qt::ApplicationState state)
		{
			obs_hotkey_enable_background_press(
				state != Qt::ApplicationActive);
		});
		obs_hotkey_enable_background_press(
			applicationState() != Qt::ApplicationActive);
		return true;
	}
	else {
		return false;
	}
}
//
// 处理讲师端退出事件通知...
void OBSApp::doLogoutEvent()
{
	// 释放远程会话对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 释放存储会话对象...
	if (m_TrackerSession != NULL) {
		delete m_TrackerSession;
		m_TrackerSession = NULL;
	}
	if (m_StorageSession != NULL) {
		delete m_StorageSession;
		m_StorageSession = NULL;
	}
	// 注意：废弃这个接口，房间状态不要通过数据库，直接通过udpserver获取...
	/*const char * lpLiveRoomID = config_get_string(this->GlobalConfig(), "General", "LiveRoomID");
	if (lpLiveRoomID == NULL)
		return;
	// 构造网络访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	QString strContentVal = QString("room_id=%1&type_id=%2").arg(QString(lpLiveRoomID)).arg(this->GetClientType());
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Gather/logoutLiveRoom");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());*/
	// 注意：这里使用curl的原因是需要阻塞式，m_objNetManager是异步，还没执行完毕就退出了...
	// 注意：废弃这个接口，房间状态不要通过数据库，直接通过udpserver获取...
	// 准备登出需要的云教室号码缓冲区...
	/*char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/logoutLiveRoom", m_strWebClass.c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", lpLiveRoomID, this->GetClientType());
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式，设置5秒超时...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}*/
}
//
// 创建并初始化登录窗口...
void OBSApp::doLoginInit()
{
	// 创建登录窗口，显示登录窗口...
	//loginWindow = new LoginWindow();
	//loginWindow->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	//connect(loginWindow, SIGNAL(loginSuccess(string&)), this, SLOT(doLoginSuccess(string&)));

	// 创建小程序二维码登录窗口...
	m_LoginMini = new CLoginMini();
	m_LoginMini->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	connect(m_LoginMini, SIGNAL(doTriggerMiniSuccess()), this, SLOT(onTriggerMiniSuccess()));
	// 关联网络信号槽反馈结果事件...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
}
//
// 处理小程序登录成功之后的信号槽事件...
void OBSApp::onTriggerMiniSuccess()
{
	// 保存登录房间号...
	int nDBRoomID = m_LoginMini->GetDBRoomID();
	m_nDBUserID = m_LoginMini->GetDBUserID();
	m_strRoomID = QString("%1").arg(nDBRoomID).toStdString();
	// 先关闭登录窗口...
	m_LoginMini->close();
	// 创建主窗口 => 失败返回...
	if (!this->OBSInit()) return;
	// 将房间号保存到obs核心对象...
	obs_set_room_id(nDBRoomID);
	// 开启一个定时上传检测时钟 => 每隔5秒执行一次...
	m_nFastTimer = this->startTimer(5 * 1000);
	// 每隔15秒检测一次，从服务器获取流量统计并存入数据库...
	m_nFlowTimer = this->startTimer(15 * 1000);
	// 每隔30秒检测一次，讲师端在中转服务器上在线汇报通知...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// 已经获取到了远程中转服务器地址，可以立即连接...
	this->doCheckRemote();
}

// 处理登录成功之后的信号槽事件...
/*void OBSApp::doLoginSuccess(string & strRoomID)
{
	// 保存登录房间号...
	m_strRoomID = strRoomID;
	// 先关闭登录窗口...
	loginWindow->close();
	// 创建主窗口 => 失败返回...
	if (!this->OBSInit()) return;
	// 将房间号保存到obs核心对象...
	obs_set_room_id(atoi(strRoomID.c_str()));
	// 开启一个定时上传检测时钟 => 每隔5秒执行一次...
	m_nFastTimer = this->startTimer(5 * 1000);
	// 每隔30秒检测一次，讲师端在中转服务器上在线汇报通知...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// 已经获取到了远程中转服务器地址，可以立即连接...
	this->doCheckRemote();
}*/

void OBSApp::onReplyFinished(QNetworkReply *reply)
{
	// 如果发生网络错误，打印错误信息，跳出循环...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		return;
	}
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	//blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
}
//
// 时钟定时执行过程...
void OBSApp::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if(nTimerID == m_nFastTimer) {
		this->doCheckFDFS();
	} else if(nTimerID == m_nOnLineTimer) {
		this->doCheckOnLine();
	} else if (nTimerID == m_nFlowTimer) {
		this->doCheckRoomFlow();
	}
}
//
// 发送在线检测命令包...
void OBSApp::doCheckOnLine()
{
	if (m_RemoteSession != NULL) {
		m_RemoteSession->doSendOnLineCmd();
	}
}
//
// 检测是否需要进行数据上传...
void OBSApp::doCheckFDFS()
{
	this->doCheckTracker();
	this->doCheckStorage();
	this->doCheckRemote();
	this->doCheckRtpSource();
}

// 遍历所有的会话资源，找到rtp_source资源，返回场景资源编号和摄像头编号...
/*void OBSApp::FindRtpSource(int & outDBCameraID, int & outSceneItemID, bool & outHasRecvThread)
{
	// 初始化返回的场景资源编号、摄像头编号、是否有拉流线程...
	outDBCameraID = 0; outSceneItemID = 0; outHasRecvThread = false;
	// 如果主窗口还没有加载完毕，不能创建远程对象，因为无法读取资源配置...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// 场景资源遍历回调函数 => 找到rtp_source，返回false，中断查找...
	auto func = [](obs_scene_t *, obs_sceneitem_t *item, void *param)
	{
		BaseSceneItem * out_item = reinterpret_cast<BaseSceneItem*>(param);
		obs_source_t * source = obs_sceneitem_get_source(item);
		// 判断是否是rtp数据源类型标志，如果是rtp资源，返回false，中断...
		const char * lpSrcID = obs_source_get_id(source);
		// 填充参数，返回false，中断查找过程...
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) {
			out_item->scene_item = item;
			return false;
		}
		// 返回true，继续下一条记录...
		return true;
	};
	// 在主窗口中变量查找第一个rtp_source资源...
	BaseSceneItem theRtpSceneItem = { 0 };
	obs_scene_enum_items(lpBasicWnd->GetCurrentScene(), func, &theRtpSceneItem);
	// 如果当前场景中有rtp_source资源，更新摄像头编号和场景资源编号...
	obs_sceneitem_t * lpRtpSceneItem = theRtpSceneItem.scene_item;
	if (lpRtpSceneItem != NULL) {
		// 先找到场景资源编号...
		outSceneItemID = (int)obs_sceneitem_get_id(lpRtpSceneItem);
		// 找到场景资源下面的资源对象，再找到对应的配置对象...
		obs_source_t * lpSource = obs_sceneitem_get_source(lpRtpSceneItem);
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		// 获取rtp_source资源是否已经创建了拉流线程标志...
		outHasRecvThread = *(bool*)obs_source_get_type_data(lpSource);
		// 找到配置里面的摄像头通道编号...
		outDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		// 注意：这里必须手动进行引用计数减少，否则，会造成内存泄漏 => obs_source_get_settings 会增加引用计数...
		obs_data_release(lpSettings);
	}
}*/
//
// 自动检测rtp_source资源，向中转服务器发送开启推流命令...
void OBSApp::doCheckRtpSource()
{
	// 如果主窗口还没有加载完毕，不能创建远程对象，因为无法读取资源配置...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// 如果远程会话无效 或 远程会话没有成功连接，直接返回...
	if (m_RemoteSession == NULL || !m_RemoteSession->IsConnected())
		return;
	// 场景资源遍历回调函数 => 找到rtp_source，执行相关操作...
	auto func = [](obs_scene_t *, obs_sceneitem_t *item, void *param)
	{
		OBSApp * obsApp = reinterpret_cast<OBSApp*>(param);
		obs_source_t * lpSource = obs_sceneitem_get_source(item);
		const char * lpSrcID = obs_source_get_id(lpSource);
		// 进行ID判断，如果不是rtp资源，返回true，继续查找...
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) != 0)
			return true;
		// 如果是rtp资源，找到场景资源编号和场景资源配置对象...
		int nSceneItemID = (int)obs_sceneitem_get_id(item);
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		// 获取rtp_source资源是否已经创建了拉流线程标志，摄像头通道编号...
		bool bHasRecvThread = obs_data_get_bool(lpSettings, "recv_thread");
		int nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		// 注意：这里必须手动进行引用计数减少，否则，会造成内存泄漏 => obs_source_get_settings 会增加引用计数...
		obs_data_release(lpSettings);
		// 如果摄像头通道无效 或 场景资源编号无效，直接返回...
		if (nDBCameraID <= 0 || nSceneItemID <= 0)
			return true;
		// 如果资源里面的拉流线程正在有效运行，直接返回...
		if (bHasRecvThread)
			return true;
		// 通过中转服务器向学生端发送开启指定通道的推流工作...
		obsApp->doSendCameraLiveStartCmd(nDBCameraID);
		// 返回true，继续下一条记录...
		return true;
	};
	// 遍历当前场景所有的互动教室资源，尝试启动拉流...
	obs_scene_enum_items(lpBasicWnd->GetCurrentScene(), func, this);
	/*// 设置登录需要的默认摄像头编号和场景资源编号...
	int nDBCameraID = 0; int nSceneItemID = 0; bool bHasRecvThread = false;
	this->FindRtpSource(nDBCameraID, nSceneItemID, bHasRecvThread);
	// 如果摄像头通道无效 或 场景资源编号无效，直接返回...
	if (nDBCameraID <= 0 || nSceneItemID <= 0)
		return;
	// 如果资源里面的拉流线程正在有效运行，直接返回...
	if (bHasRecvThread)
		return;
	// 通过中转服务器向学生端发送开启指定通道的推流工作...
	this->doSendCameraLiveStartCmd(nDBCameraID, nSceneItemID);*/
}
//
// 自动检测并创建RemoteSession...
void OBSApp::doCheckRemote()
{
	// 如果主窗口还没有加载完毕，不能创建远程对象，因为无法读取资源配置...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// 如果远程会话已经存在，并且已经连接，直接返回...
	if (m_RemoteSession != NULL && !m_RemoteSession->IsCanReBuild())
		return;
	// 判断存储服务器地址是否有效...
	if (m_strRemoteAddr.size() <= 0 || m_nRemotePort <= 0)
		return;
	// 如果会话有效，先删除之...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 初始化远程中转会话对象...
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(m_strRemoteAddr.c_str(), m_nRemotePort);
	// 关联UDP连接被服务器删除时的事件通知信号槽、获取在线通道列表的信号槽、开启或删除rtp资源的信号槽...
	this->connect(m_RemoteSession, SIGNAL(doTriggerUdpLogout(int, int, int)), lpBasicWnd, SLOT(onTriggerUdpLogout(int, int, int)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerCameraList(Json::Value&)), lpBasicWnd, SLOT(onTriggerCameraList(Json::Value&)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerRtpSource(int, bool)), lpBasicWnd, SLOT(onTriggerRtpSource(int, bool)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerCameraLiveStop(int)), lpBasicWnd, SLOT(onTriggerCameraLiveStop(int)));
}

// 获取0点位置的数据源对象的学生端推流摄像头编号...
/*int OBSApp::doZeroPosCameraPusherID()
{
	int nDBCameraID = 0;
	vec2 vPos = { 0.0f, 0.0f };
	obs_sceneitem_t * lpPosItem = OBSBasicPreview::GetItemAtPos(vPos, false);
	obs_source_t * lpSource = obs_sceneitem_get_source(lpPosItem);
	const char * lpSrcID = obs_source_get_id(lpSource);
	// 获取当前数据源的摄像头编号配置信息，注意释放配置...
	if (astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) {
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		obs_data_release(lpSettings);
	}
	return nDBCameraID;
}*/

// 向中转服务器请求当前房间在线的摄像头列表...
bool OBSApp::doSendCameraOnLineListCmd()
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraOnLineListCmd();
}

// 通过中转服务器向学生端发送停止通道推流工作...
bool OBSApp::doSendCameraLiveStopCmd(int nDBCameraID)
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraLiveStopCmd(nDBCameraID);
}

// 通过中转服务器向学生端发送开启通道推流工作...
bool OBSApp::doSendCameraLiveStartCmd(int nDBCameraID)
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraLiveStartCmd(nDBCameraID);
}

// 通过中转服务器向学生端发送云台操作命令...
bool OBSApp::doSendCameraPTZCmd(int nDBCameraID, int nCmdID, int nSpeedVal)
{
	if (m_RemoteSession == NULL || nDBCameraID <= 0)
		return false;
	return m_RemoteSession->doSendCameraPTZCmd(nDBCameraID, nCmdID, nSpeedVal);
}

bool OBSApp::doSendCameraPusherIDCmd(int nDBCameraID)
{
	if (m_RemoteSession == NULL)
		return false;
	// 注意：可以允许 nDBCameraID 是无效内容...
	return m_RemoteSession->doSendCameraPusherIDCmd(nDBCameraID);
}

// 自动检测并创建TrackerSession...
void OBSApp::doCheckTracker()
{
	// 如果对象已经创建，直接返回...
	if (m_TrackerSession != NULL)
		return;
	// 判断存储服务器地址是否有效...
	if (m_strTrackerAddr.size() <= 0 || m_nTrackerPort <= 0)
		return;
	// 初始化存储上传会话对象...
	m_TrackerSession = new CTrackerSession();
	m_TrackerSession->InitSession(m_strTrackerAddr.c_str(), m_nTrackerPort);
}
//
// 自动检测并创建StorageSession...
void OBSApp::doCheckStorage()
{
	// 首先判断一些参数的有效性...
	if (m_TrackerSession == NULL)
		return;
	// 跟踪服务器已链接，存储服务器地址已获取...
	StorageServer & theStorage = m_TrackerSession->GetStorageServer();
	if (!m_TrackerSession->IsConnected() || theStorage.port <= 0)
		return;
	// 在指定目录下查找.jpg和.mp4文件...
	char szFile[MAX_PATH] = { 0 };
	if (!this->doFindOneFile(szFile, MAX_PATH, "*.jpg") && !this->doFindOneFile(szFile, MAX_PATH, "*.mp4"))
		return;
	// 如果存储会话已经存在，正在上传中，直接返回...
	if (m_StorageSession != NULL && !m_StorageSession->IsCanReBuild())
		return;
	// 如果会话有效，先删除之...
	if (m_StorageSession != NULL) {
		delete m_StorageSession;
		m_StorageSession = NULL;
	}
	// 创建存储上传会话对象...
	m_StorageSession = new CStorageSession();
	m_StorageSession->ReBuildSession(&theStorage, szFile);
}
//
// 在指定目录查找.jpg和.mp4文件...
bool OBSApp::doFindOneFile(char * inPath, int inSize, const char * inExtend)
{
	if (inPath == NULL || inSize <= 0 || inExtend == NULL)
		return false;
	char szName[MAX_PATH] = { 0 };
	char szPath[MAX_PATH] = { 0 };
	char szFind[MAX_PATH] = { 0 };
	if (os_get_config_path(szPath, sizeof(szPath), "obs-teacher") <= 0)
		return false;
	HANDLE               handle = NULL;
	wchar_t           *  w_path = NULL;
	WIN32_FIND_DATA      wfd = { 0 };
	sprintf(szFind, "%s/%s", szPath, inExtend);
	if (os_utf8_to_wcs_ptr(szFind, 0, &w_path) <= 0)
		return false;
	// 找到第一个指定文件全路径...
	handle = FindFirstFileW(w_path, &wfd);
	if (handle == INVALID_HANDLE_VALUE) {
		bfree(w_path);
		return false;
	}
	// 释放查找句柄...
	FindClose(handle);
	// 将文件名转换成utf8格式，并进行组合输出...
	if (os_wcs_to_utf8(wfd.cFileName, 0, szName, MAX_PATH) <= 0) {
		bfree(w_path);
		return false;
	}
	// 释放空间，返回查找文件结果...
	sprintf(inPath, "%s/%s", szPath, szName);
	bfree(w_path);
	return true;
}
//
// 通过Web转发统计已登录房间流量...
void OBSApp::doCheckRoomFlow()
{
	if (m_strRoomID.size() <= 0 || m_nDBUserID <= 0 || m_nDBFlowID <= 0 || m_strRemoteAddr.size() <= 0)
		return;
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	QString strContentVal = QString("user_id=%1&room_id=%2&flow_id=%3&remote_addr=%4&remote_port=%5")
		.arg(m_nDBUserID).arg(m_strRoomID.c_str()).arg(m_nDBFlowID)
		.arg(m_strRemoteAddr.c_str()).arg(m_nRemotePort);
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Mini/roomFlow");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
}
//
// 向网站汇报上传文件结果...
void OBSApp::doWebSaveFDFS(char * lpFileName, char * lpPathFDFS, int64_t llFileSize)
{
	char szExt[32] = { 0 };
	char szDriver[32] = { 0 };
	char szDir[MAX_PATH] = { 0 };
	char szSrcName[MAX_PATH] = { 0 };
	// 分离获取到的文件全路径的组成部分...
	_splitpath(lpFileName, szDriver, szDir, szSrcName, szExt);
	// 构造网络访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	QString strContentVal = QString("ext=%1&file_src=%2&file_fdfs=%3&file_size=%4").arg(QString(szExt)).arg(QString(szSrcName)).arg(QString(lpPathFDFS)).arg(llFileSize);
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Gather/liveFDFS");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 已经废弃掉的调用curl的代码...
	/*char szExt[32] = { 0 };
	char szDriver[32] = { 0 };
	char szDir[MAX_PATH] = { 0 };
	char szSrcName[MAX_PATH] = { 0 };
	_splitpath(lpFileName, szDriver, szDir, szSrcName, szExt);
	// 准备汇报需要的缓冲区...
	char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/liveFDFS", m_strWebClass.c_str());
	sprintf(szPost, "ext=%s&file_src=%s&file_fdfs=%s&file_size=%I64d", szExt, szSrcName, lpPathFDFS, llFileSize);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式，设置5秒超时...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}*/
}

string OBSApp::GetVersionString() const
{
	stringstream ver;

#ifdef HAVE_OBSCONFIG_H
	ver << OBS_VERSION;
#else
	ver << LIBOBS_API_MAJOR_VER << "." <<
		LIBOBS_API_MINOR_VER << "." <<
		LIBOBS_API_PATCH_VER;

#endif
	ver << " (";

#ifdef _WIN32
	if (sizeof(void*) == 8)
		ver << "64bit, ";

	ver << "windows)";
#elif __APPLE__
	ver << "mac)";
#elif __FreeBSD__
	ver << "freebsd)";
#else /* assume linux for the time being */
	ver << "linux)";
#endif

	return ver.str();
}

bool OBSApp::IsPortableMode()
{
	return portable_mode;
}

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE  "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE  "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE  "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

#define DSHOW_INPUT_SOURCE    "dshow_input"
#define INTERACT_RTP_SOURCE   "rtp_source"
#define NOISE_SUPPRESS_FILTER "noise_suppress_filter"

const char *OBSApp::DShowInputSource() const
{
	return DSHOW_INPUT_SOURCE;
}

const char *OBSApp::GetNSFilter() const
{
	return NOISE_SUPPRESS_FILTER;
}

const char *OBSApp::InteractRtpSource() const
{
	return INTERACT_RTP_SOURCE;
}

const char *OBSApp::InputAudioSource() const
{
	return INPUT_AUDIO_SOURCE;
}

const char *OBSApp::OutputAudioSource() const
{
	return OUTPUT_AUDIO_SOURCE;
}

const char *OBSApp::GetLastLog() const
{
	return lastLogFile.c_str();
}

const char *OBSApp::GetCurrentLog() const
{
	return currentLogFile.c_str();
}

const char *OBSApp::GetLastCrashLog() const
{
	return lastCrashLogFile.c_str();
}

bool OBSApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}

	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

QString OBSTranslator::translate(const char *context, const char *sourceText,
	const char *disambiguation, int n) const
{
	const char *out = nullptr;
	if (!App()->TranslateString(sourceText, &out))
		return QString(sourceText);

	UNUSED_PARAMETER(context);
	UNUSED_PARAMETER(disambiguation);
	UNUSED_PARAMETER(n);
	return QT_UTF8(out);
}

static bool get_token(lexer *lex, string &str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	str.assign(token.text.array, token.text.len);
	return true;
}

static bool expect_token(lexer *lex, const char *str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	return strref_cmp(&token.text, str) == 0;
}

static uint64_t convert_log_name(bool has_prefix, const char *name)
{
	BaseLexer  lex;
	string     year, month, day, hour, minute, second;

	lexer_start(lex, name);

	if (has_prefix) {
		string temp;
		if (!get_token(lex, temp, BASETOKEN_ALPHA)) return 0;
	}

	if (!get_token(lex, year, BASETOKEN_DIGIT)) return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
	if (!get_token(lex, month, BASETOKEN_DIGIT)) return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
	if (!get_token(lex, day, BASETOKEN_DIGIT)) return 0;
	if (!get_token(lex, hour, BASETOKEN_DIGIT)) return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
	if (!get_token(lex, minute, BASETOKEN_DIGIT)) return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
	if (!get_token(lex, second, BASETOKEN_DIGIT)) return 0;

	stringstream timestring;
	timestring << year << month << day << hour << minute << second;
	return std::stoull(timestring.str());
}

static void delete_oldest_file(bool has_prefix, const char *location)
{
	BPtr<char>       logDir(GetConfigPathPtr(location));
	string           oldestLog;
	uint64_t         oldest_ts = (uint64_t)-1;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(
		App()->GlobalConfig(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir);
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts = convert_log_name(has_prefix,
				entry->d_name);

			if (ts) {
				if (ts < oldest_ts) {
					oldestLog = entry->d_name;
					oldest_ts = ts;
				}

				count++;
			}
		}

		os_closedir(dir);

		if (count > maxLogs) {
			stringstream delPath;

			delPath << logDir << "/" << oldestLog;
			os_unlink(delPath.str().c_str());
		}
	}
}

static void get_last_log(bool has_prefix, const char *subdir_to_use,
	std::string &last)
{
	BPtr<char>       logDir(GetConfigPathPtr(subdir_to_use));
	struct os_dirent *entry;
	os_dir_t         *dir = os_opendir(logDir);
	uint64_t         highest_ts = 0;

	if (dir) {
		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts = convert_log_name(has_prefix,
				entry->d_name);

			if (ts > highest_ts) {
				last = entry->d_name;
				highest_ts = ts;
			}
		}

		os_closedir(dir);
	}
}

string GenerateTimeDateFilename(const char *extension, bool noSpace)
{
	time_t    now = time(0);
	char      file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		cur_time->tm_year + 1900,
		cur_time->tm_mon + 1,
		cur_time->tm_mday,
		noSpace ? '_' : ' ',
		cur_time->tm_hour,
		cur_time->tm_min,
		cur_time->tm_sec,
		extension);

	return string(file);
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace,
	const char *format)
{
	BPtr<char> filename = os_generate_formatted_filename(extension,
		!noSpace, format);
	return string(filename);
}

vector<pair<string, string>> GetLocaleNames()
{
	string path;
	if (!GetDataFilePath("locale.ini", path))
		throw "Could not find locale.ini path";

	ConfigFile ini;
	if (ini.Open(path.c_str(), CONFIG_OPEN_EXISTING) != 0)
		throw "Could not open locale.ini";

	size_t sections = config_num_sections(ini);

	vector<pair<string, string>> names;
	names.reserve(sections);
	for (size_t i = 0; i < sections; i++) {
		const char *tag = config_get_section(ini, i);
		const char *name = config_get_string(ini, tag, "Name");
		names.emplace_back(tag, name);
	}

	return names;
}

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	get_last_log(false, "obs-teacher/logs", lastLogFile);
#ifdef _WIN32
	get_last_log(true, "obs-teacher/crashes", lastCrashLogFile);
#endif

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "obs-teacher/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath,
		ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path,
		ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "obs-teacher/logs");
		base_set_log_handler(do_log, &logFile);
	}
	else {
		blog(LOG_ERROR, "Failed to open log file");
	}
}

static auto ProfilerNameStoreRelease = [](profiler_name_store_t *store)
{
	profiler_name_store_free(store);
};

using ProfilerNameStore =
std::unique_ptr<profiler_name_store_t,
	decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
	return ProfilerNameStore{ profiler_name_store_create(),
		ProfilerNameStoreRelease };
}

static auto SnapshotRelease = [](profiler_snapshot_t *snap)
{
	profile_snapshot_free(snap);
};

using ProfilerSnapshot =
std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

ProfilerSnapshot GetSnapshot()
{
	return ProfilerSnapshot{ profile_snapshot_create(), SnapshotRelease };
}

static void SaveProfilerData(const ProfilerSnapshot &snap)
{
	if (currentLogFile.empty())
		return;

	auto pos = currentLogFile.rfind('.');
	if (pos == currentLogFile.npos)
		return;

#define LITERAL_SIZE(x) x, (sizeof(x) - 1)
	ostringstream dst;
	dst.write(LITERAL_SIZE("obs-teacher/profiler_data/"));
	dst.write(currentLogFile.c_str(), pos);
	dst.write(LITERAL_SIZE(".csv.gz"));
#undef LITERAL_SIZE

	BPtr<char> path = GetConfigPathPtr(dst.str().c_str());
	if (!profiler_snapshot_dump_csv_gz(snap.get(), path))
		blog(LOG_WARNING, "Could not save profiler data to '%s'",
			static_cast<const char*>(path));
}

static auto ProfilerFree = [](void *)
{
	profiler_stop();

	auto snap = GetSnapshot();

	profiler_print(snap.get());
	profiler_print_time_between_calls(snap.get());

	//屏蔽存盘操作，不直观，存放到日志文件...
	//SaveProfilerData(snap);

	profiler_free();
};

static const char *run_program_init = "run_program_init";
static int run_program(fstream &logFile, int argc, char *argv[])
{
	int ret = -1;

	auto profilerNameStore = CreateNameStore();

	std::unique_ptr<void, decltype(ProfilerFree)>
		prof_release(static_cast<void*>(&ProfilerFree),
			ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{ run_program_init };

	QCoreApplication::addLibraryPath(".");

	OBSApp program(argc, argv, profilerNameStore.get());
	try {
		bool created_log = false;

		program.AppInit();
		delete_oldest_file(false, "obs-teacher/profiler_data");

		OBSTranslator translator;
		program.installTranslator(&translator);

#ifdef _WIN32

		bool cancel_launch = false;
		bool already_running = false;
		RunOnceMutex rom = GetRunOnceMutex(already_running);

		if (!already_running) {
			goto run;
		}

		if (!multi) {
			QMessageBox::StandardButtons buttons(
				QMessageBox::Yes | QMessageBox::Cancel);
			QMessageBox mb(QMessageBox::Question,
				QTStr("AlreadyRunning.Title"),
				QTStr("AlreadyRunning.Text"), buttons,
				nullptr);
			mb.setButtonText(QMessageBox::Yes,
				QTStr("AlreadyRunning.LaunchAnyway"));
			mb.setButtonText(QMessageBox::Cancel, QTStr("Cancel"));
			mb.setDefaultButton(QMessageBox::Cancel);

			QMessageBox::StandardButton button;
			button = (QMessageBox::StandardButton)mb.exec();
			cancel_launch = button == QMessageBox::Cancel;
		}

		if (cancel_launch)
			return 0;

		if (!created_log) {
			create_log_file(logFile);
			created_log = true;
		}

		if (multi) {
			blog(LOG_INFO, "User enabled --multi flag and is now "
				"running multiple instances of OBS.");
		}
		else {
			blog(LOG_WARNING, "================================");
			blog(LOG_WARNING, "Warning: OBS is already running!");
			blog(LOG_WARNING, "================================");
			blog(LOG_WARNING, "User is now running multiple "
				"instances of OBS!");
		}

	run:
#endif
		// 创建日志文件...
		if (!created_log) {
			create_log_file(logFile);
			created_log = true;
		}
		// 显示所有的命令行参数...
		if (argc > 1) {
			stringstream stor; stor << argv[1];
			for (int i = 2; i < argc; ++i) {
				stor << " " << argv[i];
			}
			blog(LOG_INFO, "Command Line Arguments: %s", stor.str().c_str());
		}
		// 读取命令行各字段内容信息...
		program.doProcessCmdLine(argc, argv);

		// 等登录成功之后再调用...
		//if (!program.OBSInit())
		//	return 0;

		// 初始化登录窗口...
		program.doLoginInit();

		prof.Stop();
		return program.exec();
	}
	catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
		OBSErrorBox(nullptr, "%s", error);
	}

	return ret;
}

#define MAX_CRASH_REPORT_SIZE (150 * 1024)

#ifdef _WIN32

#define CRASH_MESSAGE \
	"Woops, Teacher has crashed!\n\nWould you like to copy the crash log " \
	"to the clipboard?  (Crash logs will still be saved to the " \
	"%appdata%\\obs-teacher\\crashes directory)"

static void main_crash_handler(const char *format, va_list args, void *param)
{
	char *text = new char[MAX_CRASH_REPORT_SIZE];

	vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
	text[MAX_CRASH_REPORT_SIZE - 1] = 0;

	delete_oldest_file(true, "obs-teacher/crashes");

	string name = "obs-teacher/crashes/Crash ";
	name += GenerateTimeDateFilename("txt");

	BPtr<char> path(GetConfigPathPtr(name.c_str()));

	fstream file;

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	file.open(wpath, ios_base::in | ios_base::out | ios_base::trunc |
		ios_base::binary);
#else
	file.open(path, ios_base::in | ios_base::out | ios_base::trunc |
		ios_base::binary);
#endif
	file << text;
	file.close();

	int ret = MessageBoxA(NULL, CRASH_MESSAGE, "Teacher has crashed!",
		MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

	if (ret == IDYES) {
		size_t len = strlen(text);

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len);
		memcpy(GlobalLock(mem), text, len);
		GlobalUnlock(mem);

		OpenClipboard(0);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
	}

	exit(-1);

	UNUSED_PARAMETER(param);
}

static void load_debug_privilege(void)
{
	const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
	TOKEN_PRIVILEGES tp;
	HANDLE token;
	LUID val;

	if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
		return;
	}

	if (!!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		AdjustTokenPrivileges(token, false, &tp,
			sizeof(tp), NULL, NULL);
	}

	CloseHandle(token);
}
#endif

#ifdef __APPLE__
#define BASE_PATH ".."
#else
#define BASE_PATH "../.."
#endif

#define CONFIG_PATH BASE_PATH "/config"

#ifndef OBS_UNIX_STRUCTURE
#define OBS_UNIX_STRUCTURE 0
#endif

int GetConfigPath(char *path, size_t size, const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		}
		else {
			return snprintf(path, size, CONFIG_PATH);
		}
	}
	else {
		return os_get_config_path(path, size, name);
	}
}

char *GetConfigPathPtr(const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		char path[512];

		if (snprintf(path, sizeof(path), CONFIG_PATH "/%s", name) > 0) {
			return bstrdup(path);
		}
		else {
			return NULL;
		}
	}
	else {
		return os_get_config_path_ptr(name);
	}
}

int GetProgramDataPath(char *path, size_t size, const char *name)
{
	return os_get_program_data_path(path, size, name);
}

char *GetProgramDataPathPtr(const char *name)
{
	return os_get_program_data_path_ptr(name);
}

bool GetFileSafeName(const char *name, std::string &file)
{
	size_t base_len = strlen(name);
	size_t len = os_utf8_to_wcs(name, base_len, nullptr, 0);
	std::wstring wfile;

	if (!len)
		return false;

	wfile.resize(len);
	os_utf8_to_wcs(name, base_len, &wfile[0], len);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t im1 = i - 1;

		if (iswspace(wfile[im1])) {
			wfile[im1] = '_';
		}
		else if (wfile[im1] != '_' && !iswalnum(wfile[im1])) {
			wfile.erase(im1, 1);
		}
	}

	if (wfile.size() == 0)
		wfile = L"characters_only";

	len = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!len)
		return false;

	file.resize(len);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &file[0], len);
	return true;
}

bool GetClosestUnusedFileName(std::string &path, const char *extension)
{
	size_t len = path.size();
	if (extension) {
		path += ".";
		path += extension;
	}

	if (!os_file_exists(path.c_str()))
		return true;

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (os_file_exists(path.c_str()));

	return true;
}

bool WindowPositionValid(QRect rect)
{
	for (QScreen* screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().intersects(rect))
			return true;
	}
	return false;
}

static inline bool arg_is(const char *arg,
	const char *long_form, const char *short_form)
{
	return (long_form  && strcmp(arg, long_form) == 0) ||
		(short_form && strcmp(arg, short_form) == 0);
}

#if !defined(_WIN32) && !defined(__APPLE__)
#define IS_UNIX 1
#endif

/* if using XDG and was previously using an older build of OBS, move config
* files to XDG directory */
#if defined(USE_XDG) && defined(IS_UNIX)
static void move_to_xdg(void)
{
	char old_path[512];
	char new_path[512];
	char *home = getenv("HOME");
	if (!home)
		return;

	if (snprintf(old_path, 512, "%s/.obs-studio", home) <= 0)
		return;

	/* make base xdg path if it doesn't already exist */
	if (GetConfigPath(new_path, 512, "") <= 0)
		return;
	if (os_mkdirs(new_path) == MKDIR_ERROR)
		return;

	if (GetConfigPath(new_path, 512, "obs-teacher") <= 0)
		return;

	if (os_file_exists(old_path) && !os_file_exists(new_path)) {
		rename(old_path, new_path);
	}
}
#endif

static bool update_ffmpeg_output(ConfigFile &config)
{
	if (config_has_user_value(config, "AdvOut", "FFOutputToFile"))
		return false;

	const char *url = config_get_string(config, "AdvOut", "FFURL");
	if (!url)
		return false;

	bool isActualURL = strstr(url, "://") != nullptr;
	if (isActualURL)
		return false;

	string urlStr = url;
	string extension;

	for (size_t i = urlStr.length(); i > 0; i--) {
		size_t idx = i - 1;

		if (urlStr[idx] == '.') {
			extension = &urlStr[i];
		}

		if (urlStr[idx] == '\\' || urlStr[idx] == '/') {
			urlStr[idx] = 0;
			break;
		}
	}

	if (urlStr.empty() || extension.empty())
		return false;

	config_remove_value(config, "AdvOut", "FFURL");
	config_set_string(config, "AdvOut", "FFFilePath", urlStr.c_str());
	config_set_string(config, "AdvOut", "FFExtension", extension.c_str());
	config_set_bool(config, "AdvOut", "FFOutputToFile", true);
	return true;
}

static bool move_reconnect_settings(ConfigFile &config, const char *sec)
{
	bool changed = false;

	if (config_has_user_value(config, sec, "Reconnect")) {
		bool reconnect = config_get_bool(config, sec, "Reconnect");
		config_set_bool(config, "Output", "Reconnect", reconnect);
		changed = true;
	}
	if (config_has_user_value(config, sec, "RetryDelay")) {
		int delay = (int)config_get_uint(config, sec, "RetryDelay");
		config_set_uint(config, "Output", "RetryDelay", delay);
		changed = true;
	}
	if (config_has_user_value(config, sec, "MaxRetries")) {
		int retries = (int)config_get_uint(config, sec, "MaxRetries");
		config_set_uint(config, "Output", "MaxRetries", retries);
		changed = true;
	}

	return changed;
}

static bool update_reconnect(ConfigFile &config)
{
	if (!config_has_user_value(config, "Output", "Mode"))
		return false;

	const char *mode = config_get_string(config, "Output", "Mode");
	if (!mode)
		return false;

	const char *section = (strcmp(mode, "Advanced") == 0) ?
		"AdvOut" : "SimpleOutput";

	if (move_reconnect_settings(config, section)) {
		config_remove_value(config, "SimpleOutput", "Reconnect");
		config_remove_value(config, "SimpleOutput", "RetryDelay");
		config_remove_value(config, "SimpleOutput", "MaxRetries");
		config_remove_value(config, "AdvOut", "Reconnect");
		config_remove_value(config, "AdvOut", "RetryDelay");
		config_remove_value(config, "AdvOut", "MaxRetries");
		return true;
	}

	return false;
}

static void convert_x264_settings(obs_data_t *data)
{
	bool use_bufsize = obs_data_get_bool(data, "use_bufsize");

	if (use_bufsize) {
		int buffer_size = (int)obs_data_get_int(data, "buffer_size");
		if (buffer_size == 0)
			obs_data_set_string(data, "rate_control", "CRF");
	}
}

static void convert_14_2_encoder_setting(const char *encoder, const char *file)
{
	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	obs_data_item_t *cbr_item = obs_data_item_byname(data, "cbr");
	obs_data_item_t *rc_item = obs_data_item_byname(data, "rate_control");
	bool modified = false;
	bool cbr = true;

	if (cbr_item) {
		cbr = obs_data_item_get_bool(cbr_item);
		obs_data_item_unset_user_value(cbr_item);

		obs_data_set_string(data, "rate_control", cbr ? "CBR" : "VBR");

		modified = true;
	}

	if (!rc_item && astrcmpi(encoder, "obs_x264") == 0) {
		if (!cbr_item)
			obs_data_set_string(data, "rate_control", "CBR");
		else if (!cbr)
			convert_x264_settings(data);

		modified = true;
	}

	if (modified)
		obs_data_save_json_safe(data, file, "tmp", "bak");

	obs_data_item_release(&rc_item);
	obs_data_item_release(&cbr_item);
	obs_data_release(data);
}

static void upgrade_settings(void)
{
	char path[512];
	int pathlen = GetConfigPath(path, 512, "obs-teacher/basic/profiles");

	if (pathlen <= 0)
		return;
	if (!os_file_exists(path))
		return;

	os_dir_t *dir = os_opendir(path);
	if (!dir)
		return;

	struct os_dirent *ent = os_readdir(dir);

	while (ent) {
		if (ent->directory && strcmp(ent->d_name, ".") != 0 &&
			strcmp(ent->d_name, "..") != 0) {
			strcat(path, "/");
			strcat(path, ent->d_name);
			strcat(path, "/basic.ini");

			ConfigFile config;
			int ret;

			ret = config.Open(path, CONFIG_OPEN_EXISTING);
			if (ret == CONFIG_SUCCESS) {
				if (update_ffmpeg_output(config) ||
					update_reconnect(config)) {
					config_save_safe(config, "tmp",
						nullptr);
				}
			}


			if (config) {
				const char *sEnc = config_get_string(config,
					"AdvOut", "Encoder");
				const char *rEnc = config_get_string(config,
					"AdvOut", "RecEncoder");

				/* replace "cbr" option with "rate_control" for
				* each profile's encoder data */
				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/recordEncoder.json");
				convert_14_2_encoder_setting(rEnc, path);

				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/streamEncoder.json");
				convert_14_2_encoder_setting(sEnc, path);
			}

			path[pathlen] = 0;
		}

		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

int main(int argc, char *argv[])
{
	// 初始化内存泄漏检测器...
	bmem_init();

#ifdef _WIN32
	obs_init_win32_crash_handler();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();
	base_set_crash_handler(main_crash_handler, nullptr);
#endif

	base_get_log_handler(&def_log_handler, nullptr);

	// 从配置读取上次设置的配置信息...
	upgrade_settings();

	fstream logFile;
	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// 释放内存泄漏检测器...
	bmem_free();

	// 最后才设置错误打印句柄，否则无法打印...
	base_set_log_handler(nullptr, nullptr);
	return ret;
}
