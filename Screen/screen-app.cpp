
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <QScreen>

#include "qt-wrappers.hpp"
#include "platform.hpp"
#include "screen-app.h"
#include "getopt.h"
#include "windows.h"
#include "OSThread.h"
#include "SocketUtils.h"

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

static int run_program(fstream &logFile, int argc, char *argv[])
{
	int ret = -1;
	QCoreApplication::addLibraryPath(".");
	CScreenApp program(argc, argv);
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
		//program.doLoginInit();
		// 执行主循环操作...
		//return program.exec();
		return 0;
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

CScreenApp::CScreenApp(int &argc, char **argv)
  : QApplication(argc, argv)
  , m_bIsDebugMode(false)
  , m_strWebCenter(DEF_WEB_CENTER)
  , m_strWebClass(DEF_WEB_CLASS)
{

}

CScreenApp::~CScreenApp()
{
	// 释放Com系统对象...
	//CoUninitialize();
}

void CScreenApp::AppInit()
{
	//if (!this->InitMacIPAddr())
	//	throw "Failed to get MAC or IP address";
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";
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
	return true;
}
