
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/profiler.hpp>

#include <QScreen>

#include "qt-wrappers.hpp"
#include "qt-display.hpp"
#include "platform.hpp"
#include "screen-app.h"
#include "getopt.h"
#include "windows.h"
#include "OSThread.h"
#include "SocketUtils.h"
#include "display-helpers.hpp"

#include "window-login-mini.h"
#include <graphics/vec4.h>
#include <graphics/matrix4.h>

#define STARTUP_SEPARATOR   "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR 	"==== Shutting down =================================================="

#define UNSUPPORTED_ERROR \
	"Failed to initialize video:\n\nRequired graphics API functionality " \
	"not found.  Your GPU may not be supported."

#define UNKNOWN_ERROR \
	"Failed to initialize video.  Your GPU may not be supported, " \
	"or your graphics drivers may need to be updated."

#define DEFAULT_LANG "zh-CN"

static log_handler_t def_log_handler;
static string currentLogFile;
static string lastLogFile;

static bool log_verbose = false;
static bool unfiltered_log = false;

bool WindowPositionValid(QRect rect)
{
	for (QScreen* screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().intersects(rect))
			return true;
	}
	return false;
}

static inline bool check_path(const char* data, const char *path, string &output)
{
	ostringstream str;
	str << path << data;
	output = str.str();

	printf("Attempted path: %s\n", output.c_str());

	return os_file_exists(output.c_str());
}

bool GetDataFilePath(const char *data, string &output)
{
	if (check_path(data, "data/screen/", output))
		return true;
	return check_path(data, OBS_DATA_PATH "/screen/", output);
}

int GetConfigPath(char *path, size_t size, const char *name)
{
	/*if (!OBS_UNIX_STRUCTURE && portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		}
		else {
			return snprintf(path, size, CONFIG_PATH);
		}
	}
	else {
		return os_get_config_path(path, size, name);
	}*/
	return os_get_config_path(path, size, name);
}

char *GetConfigPathPtr(const char *name)
{
	/*if (!OBS_UNIX_STRUCTURE && portable_mode) {
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
	}*/
	return os_get_config_path_ptr(name);
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
	char path[512] = { 0 };

	/*if (GetConfigPath(path, sizeof(path), "obs-screen/basic") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;*/

	if (GetConfigPath(path, sizeof(path), "obs-screen/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	if (GetConfigPath(path, sizeof(path), "obs-screen/crashes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-screen/updates") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
#endif

	if (GetConfigPath(path, sizeof(path), "obs-screen/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
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

			uint64_t ts = convert_log_name(has_prefix, entry->d_name);

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

static void get_last_log(bool has_prefix, const char *subdir_to_use, std::string &last)
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

string GenerateSpecifiedFilename(const char *extension, bool noSpace, const char *format)
{
	BPtr<char> filename = os_generate_formatted_filename(extension,
		!noSpace, format);
	return string(filename);
}

static inline void LogString(fstream &logFile, const char *timeString, char *str)
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

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,	const char *output_str)
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
			MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0], wNum);
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

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	get_last_log(false, "obs-screen/logs", lastLogFile);

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "obs-screen/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath, ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path, ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "obs-screen/logs");
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

using ProfilerNameStore = std::unique_ptr<profiler_name_store_t, decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
	return ProfilerNameStore{ profiler_name_store_create(),	ProfilerNameStoreRelease };
}

static auto SnapshotRelease = [](profiler_snapshot_t *snap)
{
	profile_snapshot_free(snap);
};

using ProfilerSnapshot = std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

ProfilerSnapshot GetSnapshot()
{
	return ProfilerSnapshot{ profile_snapshot_create(), SnapshotRelease };
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
		prof_release(static_cast<void*>(&ProfilerFree), ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{ run_program_init };
	QCoreApplication::addLibraryPath(".");
	CScreenApp program(argc, argv, profilerNameStore.get());

	try {
		// 初始化应用程序...
		program.AppInit();
		// 加入文字翻译对象...
		OBSTranslator translator;
		program.installTranslator(&translator);
		// 判断进程是否重复启动 => 只能启动一个进程...
		bool already_running = false;
		// 这里必须保持返回值rom的有效性，有效才能证明进程已经启动...
		RunOnceMutex rom = GetRunOnceMutex(already_running);
		// 如果进程已经启动，弹出提示框，退出...
		if (already_running) {
			OBSMessageBox::information(nullptr,
				QTStr("AlreadyRunning.Title"),
				QTStr("AlreadyRunning.Text"));
			return 0;
		}
		// 每次启动创建新的日志文件...
		create_log_file(logFile);
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
		// 初始化登录窗口...
		program.doLoginInit();
		prof.Stop();
		// 执行主循环操作...
		return program.exec();
	}
	catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
		OBSErrorBox(nullptr, "%s", error);
	}
	return ret;
}

int main(int argc, char *argv[])
{
	// 初始化网络套接字...
	WSADATA	wsData = { 0 };
	WORD	wsVersion = MAKEWORD(2, 2);
	(void)::WSAStartup(wsVersion, &wsData);

	// 初始化线程和套接字通用库...
	OSThread::Initialize();
	SocketUtils::Initialize();

	// 设置错误日志的处理句柄...
	base_get_log_handler(&def_log_handler, nullptr);

	fstream logFile;
	int nRet = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// 最后才设置错误打印句柄，否则无法打印...
	base_set_log_handler(nullptr, nullptr);

	// 释放线程和套接字通用库...
	OSThread::UnInitialize();
	SocketUtils::UnInitialize();
	return nRet;
}

string CScreenApp::GetVersionString() const
{
	stringstream ver;

	ver << OBS_VERSION;
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

QString OBSTranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const
{
	const char *out = nullptr;
	if (!App()->TranslateString(sourceText, &out))
		return QString(sourceText);
	UNUSED_PARAMETER(context);
	UNUSED_PARAMETER(disambiguation);
	UNUSED_PARAMETER(n);
	return QT_UTF8(out);
}

bool CScreenApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}
	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

// 注意：统一返回UTF8格式...
string CScreenApp::getJsonString(Json::Value & inValue)
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
void CScreenApp::doProcessCmdLine(int argc, char * argv[])
{
	int	ch = 0;
	while ((ch = getopt(argc, argv, "?hvdr")) != EOF)
	{
		switch (ch) {
		case 'd': m_bIsDebugMode = true;  continue;
		case 'r': m_bIsDebugMode = false; continue;
		case '?':
		case 'h':
		case 'v':
			blog(LOG_INFO, "-d: Run as Debug Mode => mount on Debug udpserver.");
			blog(LOG_INFO, "-r: Run as Release Mode => mount on Release udpserver.");
			break;
		}
	}
}

CScreenApp::CScreenApp(int &argc, char **argv, profiler_name_store_t *store)
  : QApplication(argc, argv)
  , profilerNameStore(store)
  , m_strWebCenter(DEF_WEB_CENTER)
  , m_strWebClass(DEF_WEB_CLASS)
{

}

CScreenApp::~CScreenApp()
{
	// 释放并打印关闭日志...
	this->ClearSceneData();
	blog(LOG_INFO, SHUTDOWN_SEPARATOR);
}

void CScreenApp::AppInit()
{
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";
}

void CScreenApp::RenderMain(void *data, uint32_t cx, uint32_t cy)
{
	CScreenApp *window = static_cast<CScreenApp*>(data);

	obs_video_info ovi = { 0 };
	obs_get_video_info(&ovi);

	window->previewCX = int(window->previewScale * float(ovi.base_width));
	window->previewCY = int(window->previewScale * float(ovi.base_height));

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height),	-100.0f, 100.0f);
	gs_set_viewport(window->previewX, window->previewY,	window->previewCX, window->previewCY);
	
	obs_render_main_texture();

	gs_projection_pop();
	gs_viewport_pop();

	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

// 创建并初始化登录窗口...
void CScreenApp::doLoginInit()
{
	// 创建小程序二维码登录窗口...
	m_LoginMini = new CLoginMini();
	m_LoginMini->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	connect(m_LoginMini, SIGNAL(doTriggerMiniSuccess()), this, SLOT(onTriggerMiniSuccess()));
	// 关联网络信号槽反馈结果事件...
	//connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));

	// 模拟登录成功之后的事件通知...
	emit m_LoginMini->doTriggerMiniSuccess();
}

// 处理小程序登录成功之后的信号槽事件...
void CScreenApp::onTriggerMiniSuccess()
{
	if (!this->OBSInit())
		return;
}

bool CScreenApp::OBSInit()
{
	char path[512] = { 0 };
	// 这个目录必须创建，屏幕捕获插件会用到这个目录...
	if (GetConfigPath(path, sizeof(path), "obs-screen/plugin_config") <= 0)
		return false;
	// 初始化obs核心 => 必须输入外部profiler_name_store_t，否则无法正常显示...
	if (!obs_startup(m_strLocale.c_str(), path, GetProfilerNameStore()))
		return false;

	// 重置底层视频资源...
	int ret = this->ResetVideo();

	switch (ret) {
	case OBS_VIDEO_MODULE_NOT_FOUND:
		throw "Failed to initialize video:  Graphics module not found";
	case OBS_VIDEO_NOT_SUPPORTED:
		throw UNSUPPORTED_ERROR;
	case OBS_VIDEO_INVALID_PARAM:
		throw "Failed to initialize video:  Invalid parameters";
	default:
		if (ret != OBS_VIDEO_SUCCESS)
			throw UNKNOWN_ERROR;
	}
	// 加载所有相关的模块...
	blog(LOG_INFO, "---------------------------------");
	obs_load_all_modules();
	// 打印模块加载日志...
	blog(LOG_INFO, "---------------------------------");
	obs_log_loaded_modules();
	// 处理延迟加载的模块...
	blog(LOG_INFO, "---------------------------------");
	obs_post_load_modules();
	// 打印启动完毕信息...
	blog(LOG_INFO, STARTUP_SEPARATOR);

	// 直接创建桌面数据源，默认第0个...
	const char * id = "monitor_capture";
	const char * name = obs_source_get_display_name(id);
	m_obsSource = obs_source_create(id, name, NULL, nullptr);

	// 创建场景对象，并将桌面数据源挂载上去...
	m_obsScene = obs_scene_create(Str("Basic.Scene"));
	this->AddSceneItem(m_obsScene, m_obsSource);

	/* set the scene as the primary draw source and go */
	obs_set_output_source(0, obs_scene_get_source(m_obsScene));

	// 第一个版本暂时屏蔽预览...
	//this->doCreateDisplay();

	// 不设定初始时间戳可以立即截图...
	//m_start_time_ns = os_gettime_ns();
	// 挂载原始YUV数据的回调接口，便于进行相关的JPG转换操作...
	obs_add_raw_video_callback(nullptr, CScreenApp::ReceiveRawVideo, this);

	// 打印所有挂载资源...
	this->LogScenes();

	return true;
}

void CScreenApp::ReceiveRawVideo(void *param, struct video_data *frame)
{
	CScreenApp * theApp = reinterpret_cast<CScreenApp*>(param);
	theApp->doSaveMemJpg(frame);
}

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

static inline enum AVPixelFormat obs_to_ffmpeg_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_NONE: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_I444: return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_I420: return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12: return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YVYU: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_YUY2: return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY: return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA: return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800: return AV_PIX_FMT_GRAY8;
	}
	return AV_PIX_FMT_NONE;
}

bool CScreenApp::doSaveMemJpg(video_data * frame)
{
	// 计算已经流逝的时间 => 秒数...
	uint64_t cur_time_ns = os_gettime_ns();
	int seconds_value = (int)((cur_time_ns - m_start_time_ns) / 1000000 / 1000);
	// 如果没有达到指定的间隔，直接返回...
	if (m_snap_second > seconds_value)
		return false;
	// 记录新的启动时间戳和压缩质量...
	m_start_time_ns = cur_time_ns;
	float qCompress = this->m_qCompress;
	// 获取系统设定的图像格式信息...
	struct obs_video_info ovi = { 0 };
	if (!obs_get_video_info(&ovi))
		return false;
	/////////////////////////////////////////////////////////////////////////
	// 注意：input->conversion 是需要变换的格式，
	// 因此，应该从 video->info 当中获取原始数据信息...
	// 同时，sws_getContext 需要AVPixelFormat而不是video_format格式...
	/////////////////////////////////////////////////////////////////////////
	// 设置ffmpeg的日志回调函数...
	//av_log_set_level(AV_LOG_VERBOSE);
	//av_log_set_callback(my_av_logoutput);
	// 统一数据源输入格式，找到压缩器需要的像素格式 => 必须是 AV_PIX_FMT_YUVJ420P 格式...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUVJ420P; //AV_PIX_FMT_YUV420P
	enum AVPixelFormat nSrcFormat = obs_to_ffmpeg_video_format(ovi.output_format);
	int nSrcWidth = ovi.output_width;
	int nSrcHeight = ovi.output_height;
	// 注意：长宽必须是4的整数倍，否则sws_scale崩溃...
	int nDstWidth = nSrcWidth / 4 * 4;
	int nDstHeight = nSrcHeight / 4 * 4;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = av_image_get_buffer_size(nDestFormat, nDstWidth, nDstHeight, 1);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	av_image_fill_arrays(pDestFrame->data, pDestFrame->linesize, pDestOutBuf, nDestFormat, nDstWidth, nDstHeight, 1);

	//nv12_to_yuv420p((const uint8_t* const*)frame->data, pDestOutBuf, nDstWidth, nDstHeight);
	// 注意：这里不用libyuv的原因是，使用sws更简单，不用根据不同像素格式调用不同接口...
	// ffmpeg自带的sws_scale转换也是没有问题的，之前有问题是由于sws_getContext的输入源需要格式AVPixelFormat，写成了video_format，造成的格式错位问题...
	// 注意：目的像素格式必须为AV_PIX_FMT_YUVJ420P，如果用AV_PIX_FMT_YUV420P格式，生成的JPG有色差，而且图像偏灰色...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nDstWidth, nDstHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, (const int*)frame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);

	// 设置转换后的数据帧内容...
	pDestFrame->width = nDstWidth;
	pDestFrame->height = nDstHeight;
	pDestFrame->format = nDestFormat;

	// 将转换后的 YUV 数据存盘成 jpg 文件...
	AVCodecContext * pOutCodecCtx = NULL;
	bool bRetSave = false;
	do {
		// 预先查找jpeg压缩器需要的输入数据格式...
		AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
		AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
		if (pOutAVCodec == NULL)
			break;
		// 创建ffmpeg压缩器的场景对象...
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// 准备数据结构需要的参数...
		pOutCodecCtx->bit_rate = 200000;
		pOutCodecCtx->width = nDstWidth;
		pOutCodecCtx->height = nDstHeight;
		// 注意：没有使用适配方式，适配出来格式有可能不是YUVJ420P，造成压缩器崩溃，因为传递的数据已经固定成YUV420P...
		// 注意：输入像素是YUV420P格式，压缩器像素是YUVJ420P格式...
		pOutCodecCtx->pix_fmt = avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, (AVPixelFormat)-1, 1, 0); //AV_PIX_FMT_YUVJ420P;
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		pOutCodecCtx->time_base.num = 1;
		pOutCodecCtx->time_base.den = 25;
		// 打开 ffmpeg 压缩器...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// 设置图像质量，默认是0.5，修改为0.8(图片太大,0.5刚刚好)...
		pOutCodecCtx->qcompress = qCompress;
		// 准备接收缓存，开始压缩jpg数据...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// 采用新的压缩函数...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pDestFrame, &got_pic);
		// 解码失败或没有得到完整图像，继续解析...
		if (nResult < 0 || !got_pic)
			break;
		// 进行JPG的内存保存 => 后续需要加互斥保护...
		m_strJPGData.assign((char*)pkt.data, pkt.size);
		// 进行JPG的存盘操作 => 这是可选的操作...
		this->doSaveFileJpg(pkt.data, pkt.size);
		// 释放分配数据块的空间...
		av_packet_unref(&pkt);
		// 返回保存成功标志...
		bRetSave = true;
	} while (false);
	// 清理中间产生的对象...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}

	// 释放临时分配的数据空间...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);

	return bRetSave;
}

bool CScreenApp::doSaveFileJpg(uint8_t * inData, int nSize)
{
	// 如果无需保存 或 输入参数无效，直接返回...
	if (!m_bIsSaveFile || inData == NULL || nSize <= 0)
		return false;
	// 获取存盘需要的配置信息 => 路径和文件名...
	char szSaveFile[100] = { 0 };
	char szSavePath[300] = { 0 };
	sprintf(szSaveFile, "obs-screen/live_%d.jpg", 200002);
	if (os_get_config_path(szSavePath, sizeof(szSavePath), szSaveFile) <= 0) {
		blog(LOG_ERROR, "doSaveFileJpg: save path error!");
		return false;
	}
	// 打开jpg文件句柄...
	FILE * pFile = fopen(szSavePath, "wb");
	if (pFile == NULL) return false;
	// 保存到磁盘，并释放资源...
	fwrite(inData, 1, nSize, pFile);
	// 释放文件句柄，返回成功...
	fclose(pFile); pFile = NULL;
	return true;
}

static void LogFilter(obs_source_t*, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	int val = (int)(intptr_t)v_val;
	string indent;

	for (int i = 0; i < val; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- filter: '%s' (%s)", indent.c_str(), name, id);
}

static bool LogSceneItem(obs_scene_t*, obs_sceneitem_t *item, void*)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);

	blog(LOG_INFO, "    - source: '%s' (%s)", name, id);

	obs_monitoring_type monitoring_type = obs_source_get_monitoring_type(source);

	if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
		const char * type = (monitoring_type == OBS_MONITORING_TYPE_MONITOR_ONLY)
			? "monitor only"
			: "monitor and output";

		blog(LOG_INFO, "        - monitoring: %s", type);
	}

	obs_source_enum_filters(source, LogFilter, (void*)(intptr_t)2);
	return true;
}

void CScreenApp::LogScenes()
{
	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Loaded scenes:");

	OBSScene scene = m_obsScene;

	obs_source_t *source = obs_scene_get_source(scene);
	const char *name = obs_source_get_name(source);

	blog(LOG_INFO, "- scene '%s':", name);
	obs_scene_enum_items(scene, LogSceneItem, nullptr);
	obs_source_enum_filters(source, LogFilter, (void*)(intptr_t)1);

	blog(LOG_INFO, "------------------------------------------------");
}

void CScreenApp::doCreateDisplay()
{
	// 注意：由于主窗口是FramelessWindowHint，随后创建的窗口会受到影响...
	// 注意：造成预览窗口无法进行拉伸和全屏操作，主窗口FramelessWindowHint造成的...
	m_preview = new OBSBasicPreview(nullptr);

	auto addDisplay = [this](OBSQTDisplay *window) {
		obs_display_add_draw_callback(window->GetDisplay(), CScreenApp::RenderMain, this);
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			this->ResizePreview(ovi.base_width, ovi.base_height);
		}
	};

	auto displayResize = [this]() {
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			this->ResizePreview(ovi.base_width, ovi.base_height);
		}
	};

	connect(m_preview, &OBSQTDisplay::DisplayCreated, addDisplay);
	connect(m_preview, &OBSQTDisplay::DisplayResized, displayResize);
}

#define PREVIEW_EDGE_SIZE	0	// 预览窗口边框间距

void CScreenApp::ResizePreview(uint32_t cx, uint32_t cy)
{
	QSize  targetSize = GetPixelSize(this->m_preview);
	GetScaleAndCenterPos(int(cx), int(cy),
		targetSize.width() - PREVIEW_EDGE_SIZE * 2,
		targetSize.height() - PREVIEW_EDGE_SIZE * 2,
		previewX, previewY, previewScale);
	previewX += float(PREVIEW_EDGE_SIZE);
	previewY += float(PREVIEW_EDGE_SIZE);
}

void CScreenApp::AddSceneItem(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t * item = nullptr;
	item = obs_scene_add(scene, source);
	obs_sceneitem_set_visible(item, true);
}

#ifdef _WIN32
#define IS_WIN32 1
#else
#define IS_WIN32 0
#endif

static inline int AttemptToResetVideo(struct obs_video_info *ovi)
{
	return obs_reset_video(ovi);
}

static inline enum obs_scale_type GetScaleType(const char *scaleTypeStr)
{
	if (astrcmpi(scaleTypeStr, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else
		return OBS_SCALE_BICUBIC;
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (astrcmpi(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (astrcmpi(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (astrcmpi(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
	else if (astrcmpi(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (astrcmpi(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (astrcmpi(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_RGBA;
}

const char *CScreenApp::GetRenderModule() const
{
	const char *renderer = config_get_string(m_globalConfig, "Video", "Renderer");
	return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
}

int CScreenApp::ResetVideo()
{
	ProfileScope("CScreenApp::ResetVideo");

	QList<QScreen*> screens = QGuiApplication::screens();

	if (!screens.size()) {
		OBSErrorBox(NULL, "There appears to be no monitors.  Er, this "
			"technically shouldn't be possible.");
		return false;
	}

	QScreen *primaryScreen = QGuiApplication::primaryScreen();
	uint32_t cx = primaryScreen->size().width();
	uint32_t cy = primaryScreen->size().height();
	/* use 1920x1080 for new default base res if main monitor is above
	* 1920x1080, but don't apply for people from older builds -- only to
	* new users */
	if ((cx * cy) > (1920 * 1080)) {
		cx = 1920; cy = 1080;
	}

	struct obs_video_info ovi = { 0 };

	// 设定颜色空间...
	const char *colorFormat = "NV12";
	const char *colorSpace = "601";
	const char *colorRange = "Partial";
	const char *scaleTypeStr = "bicubic";

	// 设定每秒帧率 => 尽量降低CPU使用率...
	ovi.fps_num = m_nFPS;
	ovi.fps_den = 1;
	
	ovi.graphics_module = App()->GetRenderModule();
	ovi.base_width = ovi.output_width = cx;
	ovi.base_height = ovi.output_height = cy;
	ovi.output_format = GetVideoFormatFromName(colorFormat);
	ovi.colorspace = astrcmpi(colorSpace, "601") == 0 ?	VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range = astrcmpi(colorRange, "Full") == 0 ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.screen_mode = true;
	ovi.scale_type = GetScaleType(scaleTypeStr);

	int ret = AttemptToResetVideo(&ovi);

	if (IS_WIN32 && ret != OBS_VIDEO_SUCCESS) {
		if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
			blog(LOG_WARNING, "Tried to reset when already active");
			return ret;
		}
		/* Try OpenGL if DirectX fails on windows */
		if (astrcmpi(ovi.graphics_module, DL_OPENGL) != 0) {
			blog(LOG_WARNING, "Failed to initialize obs video (%d) "
				"with graphics_module='%s', retrying "
				"with graphics_module='%s'",
				ret, ovi.graphics_module,
				DL_OPENGL);
			ovi.graphics_module = DL_OPENGL;
			ret = AttemptToResetVideo(&ovi);
		}
	}
	if (ret == OBS_VIDEO_SUCCESS) {
		video_t *video = obs_get_video();
		uint32_t first_encoded = video_output_get_total_frames(video);
		uint32_t first_skipped = video_output_get_skipped_frames(video);
		uint32_t first_rendered = obs_get_total_frames();
		uint32_t first_lagged = obs_get_lagged_frames();
	}
	return ret;
}

void CScreenApp::ClearSceneData()
{
	if (this->m_preview != nullptr) {
		obs_display_remove_draw_callback(m_preview->GetDisplay(), CScreenApp::RenderMain, this);
		delete this->m_preview; this->m_preview = nullptr;
	}

	obs_remove_raw_video_callback(CScreenApp::ReceiveRawVideo, this);

	obs_set_output_source(0, nullptr);
	
	auto cb = [](void *unused, obs_source_t *source) {
		obs_source_remove(source);
		UNUSED_PARAMETER(unused);
		return true;
	};

	// remove 只是设置标志，并没有删除...
	obs_enum_sources(cb, nullptr);

	obs_source_release(m_obsSource);
	obs_scene_release(m_obsScene);
	m_obsSource = nullptr;
	m_obsScene = nullptr;

	blog(LOG_INFO, "All scene data cleared");
	blog(LOG_INFO, "------------------------------------------------");
}

bool CScreenApp::InitLocale()
{
	// 获取默认的语言路径...
	string defaultLangPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", defaultLangPath)) {
		OBSErrorBox(NULL, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}
	// 创建字典对象...
	m_textLookup = text_lookup_create(defaultLangPath.c_str());
	if (!m_textLookup) {
		OBSErrorBox(NULL, "Failed to create locale from file '%s'", defaultLangPath.c_str());
		return false;
	}
	// 保存本地化 => zh-CN环境...
	m_strLocale = DEFAULT_LANG;
	return true;
}

bool CScreenApp::InitGlobalConfig()
{
	bool changed = false;
	char path[512] = { 0 };
	// 获取全局配置文件的路径 => 临时目录...
	if (GetConfigPath(path, sizeof(path), "obs-screen/global.ini") <= 0)
		return false;
	// 打开全局配置文件...
	int errorcode = m_globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}
	// 配置文件中没有网络配置参数，设置硬编码的网络配置参数...
	if (!config_has_user_value(m_globalConfig, "General", "WebCenter")) {
		config_set_string(m_globalConfig, "General", "WebCenter", DEF_WEB_CENTER);
		changed = true;
	}
	// 从配置文件当中读取中心网站的地址 => https://
	m_strWebCenter = config_get_string(m_globalConfig, "General", "WebCenter");
	// 查看节点网站地址是否有效...
	if (!config_has_user_value(m_globalConfig, "General", "WebClass")) {
		config_set_string(m_globalConfig, "General", "WebClass", DEF_WEB_CLASS);
		changed = true;
	}
	// 从配置文件当中读取云教室网站的地址 => https://
	m_strWebClass = config_get_string(m_globalConfig, "General", "WebClass");
	// 配置有变化，存盘到global.ini配置文件当中...
	if (changed) {
		config_save_safe(m_globalConfig, "tmp", nullptr);
	}
	// 返回默认的初始配置信息...
	return InitGlobalConfigDefaults();
}

bool CScreenApp::InitGlobalConfigDefaults()
{
	// 设置默认配置信息，包括自动检查升级配置信息...
	config_set_default_string(m_globalConfig, "General", "Language", DEFAULT_LANG);
	config_set_default_uint(m_globalConfig, "General", "EnableAutoUpdates", true);
	config_set_default_uint(m_globalConfig, "General", "MaxLogs", 10);

#if _WIN32
	config_set_default_string(m_globalConfig, "Video", "Renderer", "Direct3D 11");
#else
	config_set_default_string(m_globalConfig, "Video", "Renderer", "OpenGL");
#endif

	return true;
}
