
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
		// ������Ϣ����ʱ���...
		string strTemp = str;
		char szBuf[32] = { 0 };
		sprintf(szBuf, "[%I64d ms]", os_gettime_ns() / 1000000);
		sprintf(str, "%s %s", szBuf, strTemp.c_str());
		// ���п��ַ���ʽת��...
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

// �޸�Ĭ������ => zh-CN
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
// ע�⣺ͳһ����UTF8��ʽ...
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
		// ��Ҫʹ���ϰ汾��Ĭ�����ã�ֱ���趨Ϊfalse״̬...
		config_set_bool(globalConfig, "General", "Pre19Defaults", false);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre21Defaults")) {
		//uint32_t lastVersion = config_get_int(globalConfig, "General", "LastVersion");
		//bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(21, 0, 0);
		// ��Ҫʹ���ϰ汾��Ĭ�����ã�ֱ���趨Ϊfalse״̬...
		config_set_bool(globalConfig, "General", "Pre21Defaults", false);
		changed = true;
	}

	// �����ļ���û���������ò���������Ӳ������������ò���...
	if (!config_has_user_value(globalConfig, "General", "WebCenter")) {
		config_set_string(globalConfig, "General", "WebCenter", DEF_WEB_CENTER);
		changed = true;
	}
	// �������ļ����ж�ȡ������վ�ĵ�ַ...
	m_strWebCenter = config_get_string(globalConfig, "General", "WebCenter");
	// �鿴�ڵ���վ��ַ�Ƿ���Ч...
	if (!config_has_user_value(globalConfig, "General", "WebClass")) {
		config_set_string(globalConfig, "General", "WebClass", DEF_WEB_CLASS);
		changed = true;
	}
	// �������ļ����ж�ȡ�ڵ���վ�ĵ�ַ...
	m_strWebClass = config_get_string(globalConfig, "General", "WebClass");
	// �����б仯�����̵�global.ini�����ļ�����...
	if (changed) {
		config_save_safe(globalConfig, "tmp", nullptr);
	}
	// ����Ĭ�ϵĳ�ʼ������Ϣ...
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

// ע�⣺ͳһ����UTF8��ʽ...
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

// ����λ�ã���� run_program() ������ֻ����һ��...
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
	// 2018.01.11 - ��� ERROR_BUFFER_OVERFLOW ������...
	DWORD outBuflen = sizeof(IP_ADAPTER_INFO);
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO;
	DWORD retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	// ���ֻ����������ã����·���ռ�...
	if (retStatus == ERROR_BUFFER_OVERFLOW) {
		delete lpAdapter; lpAdapter = NULL;
		lpAdapter = (PIP_ADAPTER_INFO)new BYTE[outBuflen];
		retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	}
	// ���Ƿ����˴��󣬴�ӡ����ֱ�ӷ���...
	if (retStatus != ERROR_SUCCESS) {
		blog(LOG_INFO, "GetAdaptersInfo Error: %u", retStatus);
		delete lpAdapter; lpAdapter = NULL;
		return false;
	}
	// ��ʼѭ�����������ڵ�...
	IP_ADAPTER_INFO * lpInfo = lpAdapter;
	while (lpInfo != NULL) {
		// ��ӡ����������Ϣ => 2018.03.27 - �����������������...
		blog(LOG_INFO, "== Adapter Type: %lu, Desc: %s ==\n", lpInfo->Type, lpInfo->Description);
		// ��������̫������������IPV4����������������Ч������ => 71 ����������...
		if ((lpInfo->Type != MIB_IF_TYPE_ETHERNET && lpInfo->Type != 71) || lpInfo->AddressLength != 6 || strcmp(lpInfo->IpAddressList.IpAddress.String, "0.0.0.0") == 0) {
			lpInfo = lpInfo->Next;
			continue;
		}
		// ��ȡMAC��ַ��IP��ַ����ַ���໥������...
		char szBuffer[MAX_PATH] = { 0 };
		sprintf(szBuffer, "%02X-%02X-%02X-%02X-%02X-%02X", lpInfo->Address[0], lpInfo->Address[1], lpInfo->Address[2], lpInfo->Address[3], lpInfo->Address[4], lpInfo->Address[5]);
		m_strIPAddr = lpInfo->IpAddressList.IpAddress.String;
		m_strMacAddr = szBuffer;
		lpInfo = lpInfo->Next;
	}
	// �ͷŷ���Ļ�����...
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
// ����ʦ���˳��¼�֪ͨ...
void OBSApp::doLogoutEvent()
{
	// �ͷ�Զ�̻Ự����...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// �ͷŴ洢�Ự����...
	if (m_TrackerSession != NULL) {
		delete m_TrackerSession;
		m_TrackerSession = NULL;
	}
	if (m_StorageSession != NULL) {
		delete m_StorageSession;
		m_StorageSession = NULL;
	}
	// ע�⣺��������ӿڣ�����״̬��Ҫͨ�����ݿ⣬ֱ��ͨ��udpserver��ȡ...
	/*const char * lpLiveRoomID = config_get_string(this->GlobalConfig(), "General", "LiveRoomID");
	if (lpLiveRoomID == NULL)
		return;
	// ����������ʵ�ַ��������������...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	QString strContentVal = QString("room_id=%1&type_id=%2").arg(QString(lpLiveRoomID)).arg(this->GetClientType());
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Gather/logoutLiveRoom");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());*/
	// ע�⣺����ʹ��curl��ԭ������Ҫ����ʽ��m_objNetManager���첽����ûִ����Ͼ��˳���...
	// ע�⣺��������ӿڣ�����״̬��Ҫͨ�����ݿ⣬ֱ��ͨ��udpserver��ȡ...
	// ׼���ǳ���Ҫ���ƽ��Һ��뻺����...
	/*char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/logoutLiveRoom", m_strWebClass.c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", lpLiveRoomID, this->GetClientType());
	// ����Curl�ӿڣ��㱨�ɼ�����Ϣ...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// �����https://Э�飬��Ҫ��������...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// �趨curl����������postģʽ������5�볬ʱ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
		res = curl_easy_perform(curl);
	} while (false);
	// �ͷ���Դ...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}*/
}
//
// ��������ʼ����¼����...
void OBSApp::doLoginInit()
{
	// ������¼���ڣ���ʾ��¼����...
	//loginWindow = new LoginWindow();
	//loginWindow->show();
	// ������¼������Ӧ�ö�����źŲ۹�������...
	//connect(loginWindow, SIGNAL(loginSuccess(string&)), this, SLOT(doLoginSuccess(string&)));

	// ����С�����ά���¼����...
	m_LoginMini = new CLoginMini();
	m_LoginMini->show();
	// ������¼������Ӧ�ö�����źŲ۹�������...
	connect(m_LoginMini, SIGNAL(doTriggerMiniSuccess()), this, SLOT(onTriggerMiniSuccess()));
	// ���������źŲ۷�������¼�...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
}
//
// ����С�����¼�ɹ�֮����źŲ��¼�...
void OBSApp::onTriggerMiniSuccess()
{
	// �����¼�����...
	int nDBRoomID = m_LoginMini->GetDBRoomID();
	m_nDBUserID = m_LoginMini->GetDBUserID();
	m_strRoomID = QString("%1").arg(nDBRoomID).toStdString();
	// �ȹرյ�¼����...
	m_LoginMini->close();
	// ���������� => ʧ�ܷ���...
	if (!this->OBSInit()) return;
	// ������ű��浽obs���Ķ���...
	obs_set_room_id(nDBRoomID);
	// ����һ����ʱ�ϴ����ʱ�� => ÿ��5��ִ��һ��...
	m_nFastTimer = this->startTimer(5 * 1000);
	// ÿ��15����һ�Σ��ӷ�������ȡ����ͳ�Ʋ��������ݿ�...
	m_nFlowTimer = this->startTimer(15 * 1000);
	// ÿ��30����һ�Σ���ʦ������ת�����������߻㱨֪ͨ...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// �Ѿ���ȡ����Զ����ת��������ַ��������������...
	this->doCheckRemote();
}

// �����¼�ɹ�֮����źŲ��¼�...
/*void OBSApp::doLoginSuccess(string & strRoomID)
{
	// �����¼�����...
	m_strRoomID = strRoomID;
	// �ȹرյ�¼����...
	loginWindow->close();
	// ���������� => ʧ�ܷ���...
	if (!this->OBSInit()) return;
	// ������ű��浽obs���Ķ���...
	obs_set_room_id(atoi(strRoomID.c_str()));
	// ����һ����ʱ�ϴ����ʱ�� => ÿ��5��ִ��һ��...
	m_nFastTimer = this->startTimer(5 * 1000);
	// ÿ��30����һ�Σ���ʦ������ת�����������߻㱨֪ͨ...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// �Ѿ���ȡ����Զ����ת��������ַ��������������...
	this->doCheckRemote();
}*/

void OBSApp::onReplyFinished(QNetworkReply *reply)
{
	// �������������󣬴�ӡ������Ϣ������ѭ��...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		return;
	}
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	//blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
}
//
// ʱ�Ӷ�ʱִ�й���...
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
// �������߼�������...
void OBSApp::doCheckOnLine()
{
	if (m_RemoteSession != NULL) {
		m_RemoteSession->doSendOnLineCmd();
	}
}
//
// ����Ƿ���Ҫ���������ϴ�...
void OBSApp::doCheckFDFS()
{
	this->doCheckTracker();
	this->doCheckStorage();
	this->doCheckRemote();
	this->doCheckRtpSource();
}

// �������еĻỰ��Դ���ҵ�rtp_source��Դ�����س�����Դ��ź�����ͷ���...
/*void OBSApp::FindRtpSource(int & outDBCameraID, int & outSceneItemID, bool & outHasRecvThread)
{
	// ��ʼ�����صĳ�����Դ��š�����ͷ��š��Ƿ��������߳�...
	outDBCameraID = 0; outSceneItemID = 0; outHasRecvThread = false;
	// ��������ڻ�û�м�����ϣ����ܴ���Զ�̶�����Ϊ�޷���ȡ��Դ����...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// ������Դ�����ص����� => �ҵ�rtp_source������false���жϲ���...
	auto func = [](obs_scene_t *, obs_sceneitem_t *item, void *param)
	{
		BaseSceneItem * out_item = reinterpret_cast<BaseSceneItem*>(param);
		obs_source_t * source = obs_sceneitem_get_source(item);
		// �ж��Ƿ���rtp����Դ���ͱ�־�������rtp��Դ������false���ж�...
		const char * lpSrcID = obs_source_get_id(source);
		// ������������false���жϲ��ҹ���...
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) {
			out_item->scene_item = item;
			return false;
		}
		// ����true��������һ����¼...
		return true;
	};
	// ���������б������ҵ�һ��rtp_source��Դ...
	BaseSceneItem theRtpSceneItem = { 0 };
	obs_scene_enum_items(lpBasicWnd->GetCurrentScene(), func, &theRtpSceneItem);
	// �����ǰ��������rtp_source��Դ����������ͷ��źͳ�����Դ���...
	obs_sceneitem_t * lpRtpSceneItem = theRtpSceneItem.scene_item;
	if (lpRtpSceneItem != NULL) {
		// ���ҵ�������Դ���...
		outSceneItemID = (int)obs_sceneitem_get_id(lpRtpSceneItem);
		// �ҵ�������Դ�������Դ�������ҵ���Ӧ�����ö���...
		obs_source_t * lpSource = obs_sceneitem_get_source(lpRtpSceneItem);
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		// ��ȡrtp_source��Դ�Ƿ��Ѿ������������̱߳�־...
		outHasRecvThread = *(bool*)obs_source_get_type_data(lpSource);
		// �ҵ��������������ͷͨ�����...
		outDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		// ע�⣺��������ֶ��������ü������٣����򣬻�����ڴ�й© => obs_source_get_settings ���������ü���...
		obs_data_release(lpSettings);
	}
}*/
//
// �Զ����rtp_source��Դ������ת���������Ϳ�����������...
void OBSApp::doCheckRtpSource()
{
	// ��������ڻ�û�м�����ϣ����ܴ���Զ�̶�����Ϊ�޷���ȡ��Դ����...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// ���Զ�̻Ự��Ч �� Զ�̻Ựû�гɹ����ӣ�ֱ�ӷ���...
	if (m_RemoteSession == NULL || !m_RemoteSession->IsConnected())
		return;
	// ������Դ�����ص����� => �ҵ�rtp_source��ִ����ز���...
	auto func = [](obs_scene_t *, obs_sceneitem_t *item, void *param)
	{
		OBSApp * obsApp = reinterpret_cast<OBSApp*>(param);
		obs_source_t * lpSource = obs_sceneitem_get_source(item);
		const char * lpSrcID = obs_source_get_id(lpSource);
		// ����ID�жϣ��������rtp��Դ������true����������...
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) != 0)
			return true;
		// �����rtp��Դ���ҵ�������Դ��źͳ�����Դ���ö���...
		int nSceneItemID = (int)obs_sceneitem_get_id(item);
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		// ��ȡrtp_source��Դ�Ƿ��Ѿ������������̱߳�־������ͷͨ�����...
		bool bHasRecvThread = obs_data_get_bool(lpSettings, "recv_thread");
		int nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		// ע�⣺��������ֶ��������ü������٣����򣬻�����ڴ�й© => obs_source_get_settings ���������ü���...
		obs_data_release(lpSettings);
		// �������ͷͨ����Ч �� ������Դ�����Ч��ֱ�ӷ���...
		if (nDBCameraID <= 0 || nSceneItemID <= 0)
			return true;
		// �����Դ����������߳�������Ч���У�ֱ�ӷ���...
		if (bHasRecvThread)
			return true;
		// ͨ����ת��������ѧ���˷��Ϳ���ָ��ͨ������������...
		obsApp->doSendCameraLiveStartCmd(nDBCameraID);
		// ����true��������һ����¼...
		return true;
	};
	// ������ǰ�������еĻ���������Դ��������������...
	obs_scene_enum_items(lpBasicWnd->GetCurrentScene(), func, this);
	/*// ���õ�¼��Ҫ��Ĭ������ͷ��źͳ�����Դ���...
	int nDBCameraID = 0; int nSceneItemID = 0; bool bHasRecvThread = false;
	this->FindRtpSource(nDBCameraID, nSceneItemID, bHasRecvThread);
	// �������ͷͨ����Ч �� ������Դ�����Ч��ֱ�ӷ���...
	if (nDBCameraID <= 0 || nSceneItemID <= 0)
		return;
	// �����Դ����������߳�������Ч���У�ֱ�ӷ���...
	if (bHasRecvThread)
		return;
	// ͨ����ת��������ѧ���˷��Ϳ���ָ��ͨ������������...
	this->doSendCameraLiveStartCmd(nDBCameraID, nSceneItemID);*/
}
//
// �Զ���Ⲣ����RemoteSession...
void OBSApp::doCheckRemote()
{
	// ��������ڻ�û�м�����ϣ����ܴ���Զ�̶�����Ϊ�޷���ȡ��Դ����...
	OBSBasic * lpBasicWnd = qobject_cast<OBSBasic*>(mainWindow);
	if (!lpBasicWnd->IsLoaded())
		return;
	// ���Զ�̻Ự�Ѿ����ڣ������Ѿ����ӣ�ֱ�ӷ���...
	if (m_RemoteSession != NULL && !m_RemoteSession->IsCanReBuild())
		return;
	// �жϴ洢��������ַ�Ƿ���Ч...
	if (m_strRemoteAddr.size() <= 0 || m_nRemotePort <= 0)
		return;
	// ����Ự��Ч����ɾ��֮...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// ��ʼ��Զ����ת�Ự����...
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(m_strRemoteAddr.c_str(), m_nRemotePort);
	// ����UDP���ӱ�������ɾ��ʱ���¼�֪ͨ�źŲۡ���ȡ����ͨ���б���źŲۡ�������ɾ��rtp��Դ���źŲ�...
	this->connect(m_RemoteSession, SIGNAL(doTriggerUdpLogout(int, int, int)), lpBasicWnd, SLOT(onTriggerUdpLogout(int, int, int)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerCameraList(Json::Value&)), lpBasicWnd, SLOT(onTriggerCameraList(Json::Value&)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerRtpSource(int, bool)), lpBasicWnd, SLOT(onTriggerRtpSource(int, bool)));
	this->connect(m_RemoteSession, SIGNAL(doTriggerCameraLiveStop(int)), lpBasicWnd, SLOT(onTriggerCameraLiveStop(int)));
}

// ��ȡ0��λ�õ�����Դ�����ѧ������������ͷ���...
/*int OBSApp::doZeroPosCameraPusherID()
{
	int nDBCameraID = 0;
	vec2 vPos = { 0.0f, 0.0f };
	obs_sceneitem_t * lpPosItem = OBSBasicPreview::GetItemAtPos(vPos, false);
	obs_source_t * lpSource = obs_sceneitem_get_source(lpPosItem);
	const char * lpSrcID = obs_source_get_id(lpSource);
	// ��ȡ��ǰ����Դ������ͷ���������Ϣ��ע���ͷ�����...
	if (astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) {
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		obs_data_release(lpSettings);
	}
	return nDBCameraID;
}*/

// ����ת����������ǰ�������ߵ�����ͷ�б�...
bool OBSApp::doSendCameraOnLineListCmd()
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraOnLineListCmd();
}

// ͨ����ת��������ѧ���˷���ֹͣͨ����������...
bool OBSApp::doSendCameraLiveStopCmd(int nDBCameraID)
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraLiveStopCmd(nDBCameraID);
}

// ͨ����ת��������ѧ���˷��Ϳ���ͨ����������...
bool OBSApp::doSendCameraLiveStartCmd(int nDBCameraID)
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendCameraLiveStartCmd(nDBCameraID);
}

// ͨ����ת��������ѧ���˷�����̨��������...
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
	// ע�⣺�������� nDBCameraID ����Ч����...
	return m_RemoteSession->doSendCameraPusherIDCmd(nDBCameraID);
}

// �Զ���Ⲣ����TrackerSession...
void OBSApp::doCheckTracker()
{
	// ��������Ѿ�������ֱ�ӷ���...
	if (m_TrackerSession != NULL)
		return;
	// �жϴ洢��������ַ�Ƿ���Ч...
	if (m_strTrackerAddr.size() <= 0 || m_nTrackerPort <= 0)
		return;
	// ��ʼ���洢�ϴ��Ự����...
	m_TrackerSession = new CTrackerSession();
	m_TrackerSession->InitSession(m_strTrackerAddr.c_str(), m_nTrackerPort);
}
//
// �Զ���Ⲣ����StorageSession...
void OBSApp::doCheckStorage()
{
	// �����ж�һЩ��������Ч��...
	if (m_TrackerSession == NULL)
		return;
	// ���ٷ����������ӣ��洢��������ַ�ѻ�ȡ...
	StorageServer & theStorage = m_TrackerSession->GetStorageServer();
	if (!m_TrackerSession->IsConnected() || theStorage.port <= 0)
		return;
	// ��ָ��Ŀ¼�²���.jpg��.mp4�ļ�...
	char szFile[MAX_PATH] = { 0 };
	if (!this->doFindOneFile(szFile, MAX_PATH, "*.jpg") && !this->doFindOneFile(szFile, MAX_PATH, "*.mp4"))
		return;
	// ����洢�Ự�Ѿ����ڣ������ϴ��У�ֱ�ӷ���...
	if (m_StorageSession != NULL && !m_StorageSession->IsCanReBuild())
		return;
	// ����Ự��Ч����ɾ��֮...
	if (m_StorageSession != NULL) {
		delete m_StorageSession;
		m_StorageSession = NULL;
	}
	// �����洢�ϴ��Ự����...
	m_StorageSession = new CStorageSession();
	m_StorageSession->ReBuildSession(&theStorage, szFile);
}
//
// ��ָ��Ŀ¼����.jpg��.mp4�ļ�...
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
	// �ҵ���һ��ָ���ļ�ȫ·��...
	handle = FindFirstFileW(w_path, &wfd);
	if (handle == INVALID_HANDLE_VALUE) {
		bfree(w_path);
		return false;
	}
	// �ͷŲ��Ҿ��...
	FindClose(handle);
	// ���ļ���ת����utf8��ʽ��������������...
	if (os_wcs_to_utf8(wfd.cFileName, 0, szName, MAX_PATH) <= 0) {
		bfree(w_path);
		return false;
	}
	// �ͷſռ䣬���ز����ļ����...
	sprintf(inPath, "%s/%s", szPath, szName);
	bfree(w_path);
	return true;
}
//
// ͨ��Webת��ͳ���ѵ�¼��������...
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
// ����վ�㱨�ϴ��ļ����...
void OBSApp::doWebSaveFDFS(char * lpFileName, char * lpPathFDFS, int64_t llFileSize)
{
	char szExt[32] = { 0 };
	char szDriver[32] = { 0 };
	char szDir[MAX_PATH] = { 0 };
	char szSrcName[MAX_PATH] = { 0 };
	// �����ȡ�����ļ�ȫ·������ɲ���...
	_splitpath(lpFileName, szDriver, szDir, szSrcName, szExt);
	// ����������ʵ�ַ��������������...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	QString strContentVal = QString("ext=%1&file_src=%2&file_fdfs=%3&file_size=%4").arg(QString(szExt)).arg(QString(szSrcName)).arg(QString(lpPathFDFS)).arg(llFileSize);
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Gather/liveFDFS");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// �Ѿ��������ĵ���curl�Ĵ���...
	/*char szExt[32] = { 0 };
	char szDriver[32] = { 0 };
	char szDir[MAX_PATH] = { 0 };
	char szSrcName[MAX_PATH] = { 0 };
	_splitpath(lpFileName, szDriver, szDir, szSrcName, szExt);
	// ׼���㱨��Ҫ�Ļ�����...
	char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/liveFDFS", m_strWebClass.c_str());
	sprintf(szPost, "ext=%s&file_src=%s&file_fdfs=%s&file_size=%I64d", szExt, szSrcName, lpPathFDFS, llFileSize);
	// ����Curl�ӿڣ��㱨�ɼ�����Ϣ...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// �����https://Э�飬��Ҫ��������...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// �趨curl����������postģʽ������5�볬ʱ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
		res = curl_easy_perform(curl);
	} while (false);
	// �ͷ���Դ...
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

	//���δ��̲�������ֱ�ۣ���ŵ���־�ļ�...
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
		// ������־�ļ�...
		if (!created_log) {
			create_log_file(logFile);
			created_log = true;
		}
		// ��ʾ���е������в���...
		if (argc > 1) {
			stringstream stor; stor << argv[1];
			for (int i = 2; i < argc; ++i) {
				stor << " " << argv[i];
			}
			blog(LOG_INFO, "Command Line Arguments: %s", stor.str().c_str());
		}
		// ��ȡ�����и��ֶ�������Ϣ...
		program.doProcessCmdLine(argc, argv);

		// �ȵ�¼�ɹ�֮���ٵ���...
		//if (!program.OBSInit())
		//	return 0;

		// ��ʼ����¼����...
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
	// ��ʼ���ڴ�й©�����...
	bmem_init();

#ifdef _WIN32
	obs_init_win32_crash_handler();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();
	base_set_crash_handler(main_crash_handler, nullptr);
#endif

	base_get_log_handler(&def_log_handler, nullptr);

	// �����ö�ȡ�ϴ����õ�������Ϣ...
	upgrade_settings();

	fstream logFile;
	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// �ͷ��ڴ�й©�����...
	bmem_free();

	// �������ô����ӡ����������޷���ӡ...
	base_set_log_handler(nullptr, nullptr);
	return ret;
}
