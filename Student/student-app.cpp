
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <curl.h>
#include <jansson.h>

#include <QGuiApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QScreen>

#include "qt-wrappers.hpp"
#include "student-app.h"
#include "platform.hpp"
#include "windows.h"
#include "getopt.h"

#include "web-thread.h"
#include "SocketUtils.h"
#include <IPTypes.h>
#include <IPHlpApi.h>
#include <atlconv.h>
#include "SDL2/SDL.h"
#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>

#include "window-view-camera.hpp"
#include "window-view-teacher.hpp"

extern "C" {
#include "libavutil/samplefmt.h"
};

#pragma comment(lib, "iphlpapi.lib")

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
	if (check_path(data, "data/student/", output))
		return true;
	return check_path(data, OBS_DATA_PATH "/student/", output);
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

	if (GetConfigPath(path, sizeof(path), "obs-student/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	if (GetConfigPath(path, sizeof(path), "obs-student/crashes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-student/updates") <= 0)
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

	get_last_log(false, "obs-student/logs", lastLogFile);

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "obs-student/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath, ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path, ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "obs-student/logs");
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
		prof_release(static_cast<void*>(&ProfilerFree),	ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{ run_program_init };
	QCoreApplication::addLibraryPath(".");
	CStudentApp program(argc, argv, profilerNameStore.get());

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
	} catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
		OBSErrorBox(nullptr, "%s", error);
	}
	return ret;
}

/*// 回音消除和降噪的公共参数...
#define MAX_SAMPLERATE   (16000)
#define MAX_SAMPLES_10MS ((MAX_SAMPLERATE*10)/1000)

// 进行webrtc的aec测试 => float
#include "echo_cancellation.h"
#include "aec_core.h"
using namespace webrtc;
int doTestWebrtcAEC()
{
	int    err_code = 0;
	void * hAEC = NULL;
	FILE * lpMicFile = NULL;
	FILE * lpRefFile = NULL;
	FILE * lpAecFile = NULL;
	short sref[MAX_SAMPLES_10MS];
	short smic[MAX_SAMPLES_10MS];
	short saec[MAX_SAMPLES_10MS];
	float dref[MAX_SAMPLES_10MS];
	float dmic[MAX_SAMPLES_10MS];
	float daec[MAX_SAMPLES_10MS];
	int nSampleRate = MAX_SAMPLERATE;
	int nFrameSamples = MAX_SAMPLES_10MS;
	char * pMicFName = "D:\\MP4\\PCM\\nibo\\444\\IPC_16000_0_73_ns_2.pcm";
	char * pRefFName = "D:\\MP4\\PCM\\nibo\\444\\IPC_16000_1_73.pcm";
	char * pAecFName = "D:\\MP4\\PCM\\nibo\\444\\IPC_16000_2_73_aec.pcm";
	do {
		hAEC = WebRtcAec_Create();
		if (hAEC == NULL) break;
		Aec * lpAEC = (Aec*)hAEC;
		// 设置不确定延时，自动对齐波形 => 必须在初始化之前设置...
		WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
		WebRtcAec_enable_extended_filter(lpAEC->aec, true);
		// 初始化回音消除对象失败，直接返回...
		if (WebRtcAec_Init(hAEC, nSampleRate, nSampleRate) != 0)
			break;
		// 打开或创建相关PCM文件句柄...
		lpMicFile = fopen(pMicFName, "rb");
		lpRefFile = fopen(pRefFName, "rb");
		lpAecFile = fopen(pAecFName, "wb");
		if (lpMicFile == NULL || lpRefFile == NULL || lpAecFile == NULL)
			break;
		// 开始从PCM文件读取数据，并进行回音消除操作...
		while (true) {
			if (fread(smic, sizeof(short), nFrameSamples, lpMicFile) != nFrameSamples)
				break;
			if (fread(sref, sizeof(short), nFrameSamples, lpRefFile) != nFrameSamples)
				break;
			// 需要将short格式转换成float格式...
			for (int i = 0; i < nFrameSamples; i++)	{
				dref[i] = (float)sref[i];
				dmic[i] = (float)smic[i];
			}
			float* const pfmic = &(dmic[0]);
			const float* const* ppfmic = &pfmic;
			float* const pfaec = &(daec[0]);
			float* const* ppfaec = &pfaec;
			// 注意：使用自动计算延时模式，可以将msInSndCardBuf参数设置为0...
			// 先将扬声器数据进行远端投递，再投递麦克风数据进行回音消除 => 投递样本个数...
			err_code = WebRtcAec_BufferFarend(hAEC, dref, nFrameSamples);
			err_code = WebRtcAec_Process(hAEC, ppfmic, 1, ppfaec, nFrameSamples, 0, 0);
			// 将float数据格式转换成short数据格式...
			for (int i = 0; i < nFrameSamples; i++) {
				saec[i] = (short)daec[i];
			}
			// 回音消除完毕，进行存盘操作...
			fwrite(saec, sizeof(short), nFrameSamples, lpAecFile);
		}

	} while (false);
	if (lpMicFile != NULL) {
		fclose(lpMicFile);
	}
	if (lpRefFile != NULL) {
		fclose(lpRefFile);
	}
	if (lpAecFile != NULL) {
		fclose(lpAecFile);
	}
	if (hAEC != NULL) {
		WebRtcAec_Free(hAEC);
	}
	return 0;
}

// 进行webrtc的ns测试 => short
#include "noise_suppression_x.h"
int doTestWebrtcNS()
{
	FILE * lpInPCM = NULL;
	FILE * lpOutPCM = NULL;
	NsxHandle * nsInst = NULL;
	short sInFrame[MAX_SAMPLES_10MS];
	short sOutFrame[MAX_SAMPLES_10MS];
	int nSampleRate = MAX_SAMPLERATE;
	int nFrameSamples = MAX_SAMPLES_10MS;
	char * lpInFName = "D:\\MP4\\PCM\\IPC_16000_2_63.pcm";
	char * lpOutFName = "D:\\MP4\\PCM\\IPC_16000_2_63_ns_2.pcm";
	// 降噪函数，目前只能用short数据格式，不能用float数据格式...
	do {
		// 创建一个short格式的降噪对象...
		if ((nsInst = WebRtcNsx_Create()) == NULL)
			break;
		// 初始化降噪对象，使用16k频率...
		if (WebRtcNsx_Init(nsInst, nSampleRate) != 0)
			break;
		// 设定降噪程度 => 0: Mild, 1: Medium , 2: Aggressive
		if (WebRtcNsx_set_policy(nsInst, 2) != 0)
			break;
		// 打开原始音频文件，带有噪声的文件...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// 创建降噪后的文件，不带噪声的文件...
		if ((lpOutPCM = fopen(lpOutFName, "wb")) == NULL)
			break;
		while (true) {
			// 读取指定长度的噪声音频文件内容 => 10ms数据长度...
			if (fread(sInFrame, sizeof(short), nFrameSamples, lpInPCM) != nFrameSamples)
				break;
			short* const pInData = &(sInFrame[0]);
			const short* const* ppInData = &pInData;
			short* const pOutData = &(sOutFrame[0]);
			short* const* ppOutData = &pOutData;
			// 降噪处理，然后直接写入文件当中...
			WebRtcNsx_Process(nsInst, ppInData, 1, ppOutData);
			fwrite(sOutFrame, sizeof(short), nFrameSamples, lpOutPCM);
		}
	} while (false);
	if (nsInst != NULL) {
		WebRtcNsx_Free(nsInst);
	}
	if (lpInPCM != NULL) {
		fclose(lpInPCM);
	}
	if (lpOutPCM != NULL) {
		fclose(lpOutPCM);
	}
	return 0;
}*/

/*// 进行MD5文件的整体测试...
#include <util/windows/WinHandle.hpp>
#include "md5.h"
bool doTestMd5()
{
	wchar_t wFullPath[MAX_PATH] = { 0 };
	wsprintf(wFullPath, L"%s\\%s", L"../../../Release/bin/32bit/", L"Student.exe");
	WinHandle handle = CreateFile(wFullPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return false;
	vector<BYTE> buf;
	buf.resize(65536);
	MD5 md5;
	for (;;) {
		DWORD read = 0;
		if (!ReadFile(handle, buf.data(), (DWORD)buf.size(), &read, nullptr))
			return false;
		if (!read)
			break;
		md5.update((void *)buf.data(), read);
	}
	string strValue = md5.toString();
	return 0;
}*/

// 进行音频速度的整体测试...

/*// 进行音频频率转换的测试...
#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>
#define FRAME_SAMPLES 1024
int doTestAudioConvert()
{
	resample_info speed_dst = { 0 };
	resample_info speed_src = { 0 };
	audio_resampler_t * speed_resampler;
	speed_src.format = AUDIO_FORMAT_16BIT;
	speed_src.samples_per_sec = 16000;
	speed_src.speakers = SPEAKERS_MONO;
	memcpy(&speed_dst, &speed_src, sizeof(speed_dst));
	speed_dst.samples_per_sec = speed_src.samples_per_sec / 2;
	speed_resampler = audio_resampler_create(&speed_dst, &speed_src);

	FILE * lpInPCM = NULL;
	FILE * lpOutPCM = NULL;

	uint32_t  input_frames = FRAME_SAMPLES;
	short     sInFrame[FRAME_SAMPLES];
	char   szOutFName[MAX_PATH] = { 0 };
	char * lpInFName = "F:\\MP4\\PCM\\src_16000_1_16.pcm";
	sprintf(szOutFName, "F:\\MP4\\PCM\\dst_%d_1_16.pcm", speed_dst.samples_per_sec);

	do {
		// 打开原始音频文件...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// 创建降处理后的文件...
		if ((lpOutPCM = fopen(szOutFName, "wb")) == NULL)
			break;
		while (true) {
			// 读取指定长度的噪声音频文件内容...
			if (fread(sInFrame, sizeof(short), input_frames, lpInPCM) != input_frames)
				break;
			uint64_t  ts_offset = 0;
			uint32_t  output_frames = 0;
			uint8_t * output_data[MAX_AV_PLANES] = { 0 };
			uint8_t * input_data[MAX_AV_PLANES] = { 0 };
			input_data[0] = (uint8_t *)&sInFrame;
			if (audio_resampler_resample(speed_resampler, output_data, &output_frames, &ts_offset, input_data, input_frames)) {
				int cur_data_size = get_audio_size(speed_dst.format, speed_dst.speakers, output_frames);
				fwrite(output_data[0], 1, cur_data_size, lpOutPCM);
			}
		}
	} while (false);
	if (speed_resampler != nullptr) {
		audio_resampler_destroy(speed_resampler);
		speed_resampler = nullptr;
	}
	if (lpInPCM != NULL) {
		fclose(lpInPCM);
	}
	if (lpOutPCM != NULL) {
		fclose(lpOutPCM);
	}
	return 0;
}*/

/*// 进行音频速度的整体测试...
#include "sonic.h"
#define JUMP_NUM       1
#define FRAME_SAMPLES  6780
int doTestAudioSpeed()
{
	sonicStream stream;
	int sampleRate = 16000;
	int numChannels = 1;
	float speed = 1.5f;
	float pitch = 1.0f;
	float rate = 1.0f;
	float volume = 1.0f;
	int quality = 0;

	stream = sonicCreateStream(sampleRate, numChannels);
	sonicSetSpeed(stream, speed);
	sonicSetPitch(stream, pitch);
	sonicSetRate(stream, rate);
	sonicSetVolume(stream, volume);
	sonicSetQuality(stream, quality);
	//sonicSetChordPitch(stream, 1);

	int nState = 0;
	int nStepNum = 0;

	FILE * lpInPCM = NULL;
	FILE * lpOutPCM = NULL;
	int    input_frames = FRAME_SAMPLES;
	short  sInFrame[FRAME_SAMPLES];
	short  sOutFrame[FRAME_SAMPLES*3];
	char   szOutFName[MAX_PATH] = { 0 };
	char * lpInFName = "F:\\MP4\\PCM\\src_16000_1_16.pcm";
	sprintf(szOutFName, "F:\\MP4\\PCM\\dst_16000_1_16_%.1f.pcm", speed);
	do {
		// 打开原始音频文件...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// 创建降处理后的文件...
		if ((lpOutPCM = fopen(szOutFName, "wb")) == NULL)
			break;
		float fTotalTime = 0.0f;
		int   samplesReaded = 0;
		int   samplesWritten = 0;
		while (true) {
			// 读取指定长度的噪声音频文件内容...
			samplesReaded = fread(sInFrame, sizeof(short), input_frames, lpInPCM);
			if (samplesReaded <= 0) {
				// 读取到末尾，刷新残留...
				sonicFlushStream(stream);
				// 读取最后的残留数据内容...
				samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
				if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
				fTotalTime += samplesWritten / (sampleRate * 1.0f);
				break;
			}
			// 加速X个包，再恢复X个包，然后再减速X个包，往复循环...
			if (nState == 0) {
				if (nStepNum++ >= JUMP_NUM) {
					// 变速调整...
					speed = 1.0;
					// 变速发生变化时，刷新残留 => 由于会填充空白数据，不要轻易使用...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// 设定新的状态和变速...
					nState = 1; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			} else if (nState == 1) {
				if (nStepNum++ >= JUMP_NUM) {
					// 变速调整...
					speed = 0.5;
					// 变速发生变化时，刷新残留 => 由于会填充空白数据，不要轻易使用...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// 设定新的状态和变速...
					nState = 2; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			} else if (nState == 2) {
				if (nStepNum++ >= JUMP_NUM) {
					// 变速调整...
					speed = 2.0;
					// 变速发生变化时，刷新残留 => 由于会填充空白数据，不要轻易使用...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// 设定新的状态和变速...
					nState = 0; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			}
			// 写入新的原始音频数据，必须保证在相同速度下没有其它速度残留...
			sonicWriteShortToStream(stream, sInFrame, samplesReaded);
			samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES*3);
			if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
			fTotalTime += samplesWritten / (sampleRate * 1.0f);
		}
	} while (false);
	if (stream != NULL) {
		sonicDestroyStream(stream);
	}
	if (lpInPCM != NULL) {
		fclose(lpInPCM);
	}
	if (lpOutPCM != NULL) {
		fclose(lpOutPCM);
	}
	return 0;
}*/

/*int doSplitAudio()
{
	char * url = "F:\\MP4\\PCM\\horn_mp3_2_float.pcm";
	ifstream infp;
	infp.open(url, ios::in | ios::binary);
	ofstream outlefp, outrifp;
	outlefp.open("F:\\MP4\\PCM\\horn_mp3_outleft.pcm", ios::out | ios::binary);
	outrifp.open("F:\\MP4\\PCM\\horn_mp3_outright.pcm", ios::out | ios::binary);

	unsigned char* pcmbuf = (unsigned char*)malloc(8);
	while (!infp.eof())
	{
		infp.read((char*)pcmbuf, 8);
		outlefp.write((const char*)pcmbuf, 4);
		outrifp.write((const char*)pcmbuf + 4, 4);
	}
	free(pcmbuf);
	infp.close();
	outlefp.close();
	outrifp.close();
	return 0;
}

int doMixerAudio()
{
	ifstream infp1, infp2;
	infp1.open("F:\\MP4\\PCM\\horn_av1_2_float.pcm", ios::in | ios::binary);
	infp2.open("F:\\MP4\\PCM\\horn_mp3_2_float.pcm", ios::in | ios::binary);
	ofstream outlefp;
	outlefp.open("F:\\MP4\\PCM\\horn_mixer_float.pcm", ios::out | ios::binary);
	float fIn1, fIn2, fOut;
	while (!infp1.eof() && !infp2.eof()) {
		infp1.read((char*)&fIn1, 4);
		infp2.read((char*)&fIn2, 4);
		fOut = fIn1 + fIn2;
		outlefp.write((char*)&fOut, 4);
	}
	infp1.close();
	infp2.close();
	outlefp.close();
	return 0;
}*/

int main(int argc, char *argv[])
{
	// 进行左右声道分离测试...
	//return doSplitAudio();
	// 进行单声道的混音测试...
	//return doMixerAudio();
	// 进行webrtc的aed测试...
	//return doTestWebrtcAEC();
	// 进行webrtc的ns测试...
	//return doTestWebrtcNS();
	// 进行MD5文件的整体测试...
	//if ((argc > 1 && stricmp(argv[1], "-upload") == 0) ||
	//	(argc > 2 && stricmp(argv[2], "-upload") == 0) )
	//	return doTestMd5();
	// 进行音频频率转换的测试...
	//return doTestAudioConvert();
	// 进行音频速度的整体测试...
	//return doTestAudioSpeed();

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
	curl_global_init(CURL_GLOBAL_ALL);
	int nRet = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// 最后才设置错误打印句柄，否则无法打印...
	base_set_log_handler(nullptr, nullptr);

	// 释放线程和套接字通用库...
	OSThread::UnInitialize();
	SocketUtils::UnInitialize();
	return nRet;
}

string CStudentApp::GetVersionString() const
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

bool CStudentApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}
	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

// 注意：统一返回UTF8格式...
string CStudentApp::getJsonString(Json::Value & inValue)
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

// 注意：统一返回UTF8格式...
char * CStudentApp::GetServerDNSName()
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

// 注意：统一返回UTF8格式...
char * CStudentApp::GetServerOS()
{
	static char szBuffer[MAX_PATH] = { 0 };
	if (strlen(szBuffer) > 0)
		return szBuffer;
	::sprintf(szBuffer, "%s", CStudentApp::GetSystemVer().c_str());
	assert(::strlen(szBuffer) <= MAX_PATH);
	return szBuffer;
}

// 注意：统一返回UTF8格式...
string CStudentApp::GetSystemVer()
{
	string	strVersion = "Windows Unknown";
	OSVERSIONINFO osv = { 0 };
	static char szBuffer[MAX_PATH] = { 0 };
	osv.dwOSVersionInfoSize = sizeof(osv);
	if (!GetVersionEx(&osv))
		return strVersion;
	static char szTempVer[MAX_PATH] = { 0 };
	os_wcs_to_utf8(osv.szCSDVersion, 128, szTempVer, MAX_PATH);
	sprintf(szBuffer, "Windows %ld.%ld.%ld %s", osv.dwMajorVersion, osv.dwMinorVersion, osv.dwBuildNumber, szTempVer);
	strVersion = szBuffer;
	return strVersion;
}

// 调用位置，详见 run_program() 函数，只调用一次...
void CStudentApp::doProcessCmdLine(int argc, char * argv[])
{
	int	ch = 0;
	while ((ch = getopt(argc, argv, "?hvdrz")) != EOF)
	{
		switch (ch) {
		case 'd': m_bIsDebugMode = true;  continue;
		case 'r': m_bIsDebugMode = false; continue;
		case 'z': m_strThirdURI = argv[optind++]; optreset=1; continue;
		case '?':
		case 'h':
		case 'v':
			blog(LOG_INFO, "-d: Run as Debug Mode => mount on Debug udpserver.");
			blog(LOG_INFO, "-r: Run as Release Mode => mount on Release udpserver.");
			break;
		}
	}
}

CStudentApp::CStudentApp(int &argc, char **argv, profiler_name_store_t *store)
  : QApplication(argc, argv)
  , profilerNameStore(store)
  , m_lpWebThread(NULL)
  , m_lpFocusDisplay(NULL)
  , m_bSaveAECSample(false)
  , m_bHasAudioHorn(false)
  , m_bAuthLicense(false)
  , m_bIsDebugMode(false)
  , m_nDBUserID(0)
  , m_nDBFlowID(0)
  , m_nUpFlowByte(0)
  , m_nDownFlowByte(0)
  , m_nRtpTCPSockFD(0)
  , m_nFlowTeacherID(0)
  , m_nRoleType(kRoleWanRecv)
  , m_nMaxCamera(DEF_MAX_CAMERA)
  , m_strWebCenter(DEF_WEB_CENTER)
  , m_strWebClass(DEF_WEB_CLASS)
  , m_nDBHaoYiGatherID(-1)
  , m_nDBHaoYiNodeID(-1)
  , m_strAuthExpired("")
  , m_strTrackerAddr("")
  , m_strRemoteAddr("")
  , m_nTrackerPort(0)
  , m_nDBGatherID(-1)
  , m_nRemotePort(0)
  , m_strMainName("")
  , m_strWebName("")
  , m_strWebTag("")
  , m_strWebVer("")
  , m_nWebType(-1)
  , m_nAuthDays(0)
  , m_nSnapVal(2)
{
	m_nFlowTimer = -1;
	m_nFastTimer = -1;
	m_nOnLineTimer = -1;
	m_LoginMini = NULL;
	m_RemoteSession = NULL;
	m_studentWindow = NULL;
	// 设置默认的音频播放、输出采样率，输出声道数...
	m_nAudioOutSampleRate = DEF_AUDIO_OUT_SAMPLE_RATE;
	m_nAudioOutChannelNum = DEF_AUDIO_OUT_CHANNEL_NUM;
	m_nAudioOutBitrateAAC = DEF_AUDIO_OUT_BITRATE_AAC;
}

CStudentApp::~CStudentApp()
{
	// 删除TCP长链接对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession;
	}
	// 网站线程有效，直接删除...
	if (m_lpWebThread != NULL) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// 释放Com系统对象...
	CoUninitialize();
}

QString CStudentApp::GetRoleString()
{
	QString strRole = QTStr("Student.RoleWanRecv");
	switch (m_nRoleType)
	{
	case kRoleWanRecv:   strRole = QTStr("Student.RoleWanRecv");   break;
	case kRoleMultiRecv: strRole = QTStr("Student.RoleMultiRecv"); break;
	case kRoleMultiSend: strRole = QTStr("Student.RoleMultiSend"); break;
	}
	return strRole;
}

int CStudentApp::GetAudioRateIndex()
{
	int inAudioRate = m_nAudioOutSampleRate;
	int audio_rate_index = 0x0;
	if (inAudioRate == 48000)
		audio_rate_index = 0x03;
	else if (inAudioRate == 44100)
		audio_rate_index = 0x04;
	else if (inAudioRate == 32000)
		audio_rate_index = 0x05;
	else if (inAudioRate == 24000)
		audio_rate_index = 0x06;
	else if (inAudioRate == 22050)
		audio_rate_index = 0x07;
	else if (inAudioRate == 16000)
		audio_rate_index = 0x08;
	else if (inAudioRate == 12000)
		audio_rate_index = 0x09;
	else if (inAudioRate == 11025)
		audio_rate_index = 0x0a;
	else if (inAudioRate == 8000)
		audio_rate_index = 0x0b;
	return audio_rate_index;
}

// 向左侧通道投递扬声器音频数据内容...
void CStudentApp::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	if (m_studentWindow != NULL) {
		CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
		((lpViewLeft != NULL) ? lpViewLeft->doEchoCancel(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf) : NULL);
	}
}

// 通知左侧窗口，接收播放线程已经停止了...
void CStudentApp::onUdpRecvThreadStop()
{
	if (m_studentWindow != NULL) {
		CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
		((lpViewLeft != NULL) ? lpViewLeft->onUdpRecvThreadStop() : NULL);
	}
}

bool CStudentApp::InitMacIPAddr()
{
	// 2018.01.11 - 解决 ERROR_BUFFER_OVERFLOW 的问题...
	DWORD outBuflen = sizeof(IP_ADAPTER_INFO);
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO;
	DWORD retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	// 发现缓冲区不够用，重新分配空间...
	if (retStatus == ERROR_BUFFER_OVERFLOW) {
		MsgLogGM(retStatus);
		delete lpAdapter; lpAdapter = NULL;
		lpAdapter = (PIP_ADAPTER_INFO)new BYTE[outBuflen];
		retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	}
	// 还是发生了错误，打印错误，直接返回...
	if (retStatus != ERROR_SUCCESS) {
		delete lpAdapter; lpAdapter = NULL;
		MsgLogGM(retStatus);
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

void CStudentApp::AppInit()
{
	if (!this->InitMacIPAddr())
		throw "Failed to get MAC or IP address";
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";
}

bool CStudentApp::InitLocale()
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

bool CStudentApp::InitGlobalConfig()
{
	bool changed = false;
	char path[512] = { 0 };
	// 获取全局配置文件的路径 => 临时目录...
	if (GetConfigPath(path, sizeof(path), "obs-student/global.ini") <= 0 )
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
	// 查看配置文件中是否有保存AEC样本数据标志配置字段...
	if (!config_has_user_value(m_globalConfig, "General", "SaveAECSample")) {
		config_set_bool(m_globalConfig, "General", "SaveAECSample", false);
		changed = true;
	}
	// 从全局配置文件中读取是否保存音频回音消除样本数据...
	m_bSaveAECSample = config_get_bool(m_globalConfig, "General", "SaveAECSample");
	// 配置有变化，存盘到global.ini配置文件当中...
	if (changed) {
		config_save_safe(m_globalConfig, "tmp", nullptr);
	}
	// 返回默认的初始配置信息...
	return InitGlobalConfigDefaults();
}

bool CStudentApp::InitGlobalConfigDefaults()
{
	// 设置默认配置信息，包括自动检查升级配置信息...
	config_set_default_string(m_globalConfig, "General", "Language", DEFAULT_LANG);
	config_set_default_uint(m_globalConfig, "General", "EnableAutoUpdates", true);
	config_set_default_uint(m_globalConfig, "General", "MaxLogs", 10);
	return true;
}

// 创建并初始化登录窗口...
void CStudentApp::doLoginInit()
{
	// 创建登录窗口，显示登录窗口...
	//m_RoomWindow = new LoginWindow();
	//m_RoomWindow->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	//connect(m_RoomWindow, SIGNAL(doTriggerLoginSuccess(string&)), this, SLOT(onTriggerLoginSuccess(string&)));

	// 创建小程序二维码登录窗口...
	m_LoginMini = new CLoginMini();
	m_LoginMini->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	connect(m_LoginMini, SIGNAL(doTriggerMiniSuccess()), this, SLOT(onTriggerMiniSuccess()));
	// 关联网络信号槽反馈结果事件...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
}

// 处理小程序登录成功之后的信号槽事件...
void CStudentApp::onTriggerMiniSuccess()
{
	// 保存登录房间号...
	int nDBRoomID = m_LoginMini->GetDBRoomID();
	m_nDBUserID = m_LoginMini->GetDBUserID();
	m_strRoomID = QString("%1").arg(nDBRoomID).toStdString();
	// 先关闭小程序登录窗口...
	m_LoginMini->close();
	// 创建并打开主窗口界面...
	setQuitOnLastWindowClosed(false);
	m_studentWindow = new StudentWindow();
	m_studentWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	this->connect(m_studentWindow, SIGNAL(destroyed()), this, SLOT(quit()));
	m_studentWindow->InitWindow();
	// 建立信号槽关联函数 => 便于辅助线程进行事件通知...
	this->connect(this, SIGNAL(msgFromWebThread(int, int, int)), m_studentWindow, SLOT(doWebThreadMsg(int, int, int)));
	this->connect(this, SIGNAL(doEnableCamera(OBSQTDisplay*)), m_studentWindow->GetViewLeft(), SLOT(doEnableCamera(OBSQTDisplay*)));
	// 创建并启动一个网站交互线程...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
}

// 处理房间登录成功之后的信号槽事件...
/*void CStudentApp::onTriggerLoginSuccess(string & strRoomID)
{
	// 保存登录房间号...
	m_strRoomID = strRoomID;
	// 关闭房间登录窗口...
	m_RoomWindow->close();
	// 创建并打开主窗口界面...
	setQuitOnLastWindowClosed(false);
	m_studentWindow = new StudentWindow();
	m_studentWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	this->connect(m_studentWindow, SIGNAL(destroyed()), this, SLOT(quit()));
	m_studentWindow->InitWindow();
	// 建立信号槽关联函数 => 便于辅助线程进行事件通知...
	this->connect(this, SIGNAL(msgFromWebThread(int, int, int)), m_studentWindow, SLOT(doWebThreadMsg(int, int, int)));
	this->connect(this, SIGNAL(doEnableCamera(OBSQTDisplay*)), m_studentWindow->GetViewLeft(), SLOT(doEnableCamera(OBSQTDisplay*)));
	// 创建并启动一个网站交互线程...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
}*/

// 登录成功获取终端配置之后，立即连接中转服务器...
void CStudentApp::onWebLoadResource()
{
	// 创建远程中转服务器对象...
	this->doCheckRemote();
	// 开启一个定时检测时钟 => 每隔5秒执行一次...
	m_nFastTimer = this->startTimer(5 * 1000);
	// 每隔15秒检测一次，从服务器获取流量统计并存入数据库...
	m_nFlowTimer = this->startTimer(15 * 1000);
	// 每隔30秒检测一次，学生端在中转服务器上在线通道列表...
	m_nOnLineTimer = this->startTimer(30 * 1000);
}

// 处理学生端授权过期的情况...
void CStudentApp::onWebAuthExpired()
{
	this->doLogoutEvent();
}

// 时钟定时执行过程...
void CStudentApp::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nFastTimer) {
		this->doCheckFDFS();
	} else if (nTimerID == m_nOnLineTimer) {
		this->doCheckOnLine();
	} else if (nTimerID == m_nFlowTimer) {
		this->doCheckRoomFlow();
	}
}

// 响应全局的键盘事件，专门处理音量的放大缩小操作...
bool CStudentApp::notify(QObject * inObject, QEvent * inEvent)
{
	// 对键盘按下事件进行预处理操作...
	if (inEvent->type() == QEvent::KeyPress) {
		QKeyEvent * keyEvent = static_cast<QKeyEvent*>(inEvent);
		int nKeyItem = keyEvent->key();
		// 主窗口有效是才处理，专门过滤音量的放大或缩小事件，正确响应，不再传递...
		if ((m_studentWindow != NULL) && m_studentWindow->VolumeEvent(inObject, nKeyItem))
			return true;
		// 没有正确响应键盘事件，需要继续传递，处理键盘响应...
	}
	return QApplication::notify(inObject, inEvent);
}

void CStudentApp::doCheckFDFS()
{
	this->doCheckRemote();
}

void CStudentApp::doCheckRemote()
{
	// 判断远程中转服务器地址是否有效...
	if (m_strRemoteAddr.size() <= 0 || m_nRemotePort <= 0)
		return;
	// 如果远程会话已经存在，并且已经连接，直接返回...
	if (m_RemoteSession != NULL && !m_RemoteSession->IsCanReBuild())
		return;
	// 如果会话有效，先删除之...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 初始化远程中转会话对象...
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(m_strRemoteAddr.c_str(), m_nRemotePort);
	// 将远程会话的信号槽与主窗口对象进行相互关联操作 => UDP连接断开时的事件通知...
	connect(m_RemoteSession, SIGNAL(doTriggerUdpLogout(int, int, int)), m_studentWindow, SLOT(onTriggerUdpLogout(int, int, int)));
	connect(m_RemoteSession, SIGNAL(doTriggerCameraPTZ(int, int, int)), m_studentWindow, SLOT(onTriggerCameraPTZ(int, int, int)));
	// 建立左侧窗口与发送线程事件、连接服务器成功的信号槽...
	CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
	connect(m_RemoteSession, SIGNAL(doTriggerConnected()), lpViewLeft, SLOT(onTriggerConnected()));
	connect(m_RemoteSession, SIGNAL(doTriggerLiveStop(int)), lpViewLeft, SLOT(onTriggerLiveStop(int)));
	connect(m_RemoteSession, SIGNAL(doTriggerLiveStart(int)), lpViewLeft, SLOT(onTriggerLiveStart(int)));
	// 建立远程会话与老师窗口对象的信号槽关联函数...
	CViewRight * lpViewRight = m_studentWindow->GetViewRight();
	CViewTeacher * lpViewTeacher = lpViewRight->GetViewTeacher();
	// 将远程会话的信号槽进行相互关联 => 老师推流端上线或下线时的事件通知...
	if (lpViewTeacher != NULL) {
		connect(m_RemoteSession, SIGNAL(doTriggerRecvThread(bool)), lpViewTeacher, SLOT(onTriggerUdpRecvThread(bool)));
		connect(m_RemoteSession, SIGNAL(doTriggerDeleteExAudioThread()), lpViewTeacher, SLOT(onTriggerDeleteExAudioThread()));
	}
}

// 通过Web转发统计已登录房间流量...
void CStudentApp::doCheckRoomFlow()
{
	if (m_nDBGatherID <= 0 || m_nDBUserID <= 0 || m_nDBFlowID <= 0 )
		return;
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebClass = App()->GetWebClass();
	int nUpFlowMB = m_nUpFlowByte / 1000 / 1000;
	int nDownFlowMB = m_nDownFlowByte / 1000 / 1000;
	QString strContentVal = QString("flow_id=%1&gather_id=%2&flow_teacher=%3&up_flow=%4&down_flow=%5")
		.arg(m_nDBFlowID).arg(m_nDBGatherID).arg(m_nFlowTeacherID).arg(nUpFlowMB).arg(nDownFlowMB);
	QString strRequestURL = QString("%1%2").arg(strWebClass.c_str()).arg("/wxapi.php/Mini/gatherFlow");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
}

void CStudentApp::onReplyFinished(QNetworkReply *reply)
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

// 返回左侧是否处于正在推流状态...
bool CStudentApp::IsLeftPusher()
{
	CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
	return lpViewLeft->IsLeftPusher();
}

void CStudentApp::doCheckOnLine()
{
	if (m_RemoteSession == NULL)
		return;
	m_RemoteSession->doSendOnLineCmd();
}

// 重建系统资源...
void CStudentApp::doReBuildResource()
{
	// 退出并删除相关资源...
	this->doLogoutEvent();
	// 创建并启动一个网站交互线程...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// 开启一个定时检测时钟 => 每隔5秒执行一次...
	m_nFastTimer = this->startTimer(5 * 1000);
	// 每隔30秒检测一次，学生端在中转服务器上在线通道列表...
	m_nOnLineTimer = this->startTimer(30 * 1000);
}
//
// 处理学生端退出事件通知...
void CStudentApp::doLogoutEvent()
{
	// 删除检测时钟...
	(m_nFastTimer >= 0) ? this->killTimer(m_nFastTimer) : NULL;
	(m_nOnLineTimer >= 0) ? this->killTimer(m_nOnLineTimer) : NULL;
	m_nOnLineTimer = m_nFastTimer = -1;
	// 删除TCP长链接对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession;
	}
	// 通知网站学生端退出...
	if (m_lpWebThread != NULL) {
		m_lpWebThread->doWebGatherLogout();
	}
	// 网站线程有效，直接删除...
	if (m_lpWebThread != NULL) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// 直接重置焦点窗口对象...
	m_lpFocusDisplay = NULL;
	// 重置通道配置集合对象...
	m_MapNodeCamera.clear();
}

// 摄像头窗口被删除时会被调用 => 如果自己就是那个焦点，需要重置...
void CStudentApp::doResetFocus(OBSQTDisplay * lpCurDisplay)
{
	// 如果输入指针无效或与当前焦点对象不一致，直接返回...
	if ((lpCurDisplay == NULL) || (m_lpFocusDisplay != lpCurDisplay))
		return;
	// 重置焦点窗口对象...
	m_lpFocusDisplay = NULL;
}

void CStudentApp::doSaveFocus(OBSQTDisplay * lpNewDisplay)
{
	// 如果指针无效或相同，直接返回...
	if ((lpNewDisplay == NULL) || (m_lpFocusDisplay == lpNewDisplay))
		return;
	// 之前的焦点有效，释放焦点...
	if (m_lpFocusDisplay != NULL) {
		m_lpFocusDisplay->doReleaseFocus();
	}
	// 保存为新的焦点对象...
	m_lpFocusDisplay = lpNewDisplay;
	// 如果新焦点是CViewCamera对象...
	CViewCamera * lpViewCamera = dynamic_cast<CViewCamera*>(lpNewDisplay);
	if (lpViewCamera == NULL)
		return;
	int nDBCameraID = lpViewCamera->GetDBCameraID();
	this->doUpdatePTZ(nDBCameraID);
}

void CStudentApp::doUpdatePTZ(int nDBCameraID)
{
	m_studentWindow->doUpdatePTZ(nDBCameraID);
}

void CStudentApp::doDelCamera(int nDBCameraID)
{
	m_MapNodeCamera.erase(nDBCameraID);
}

string CStudentApp::GetCameraDeviceSN(int nDBCameraID)
{
	string strDeviceSN;
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	if (itorItem == m_MapNodeCamera.end())
		return strDeviceSN;
	GM_MapData & theMapData = itorItem->second;
	return theMapData["device_sn"];
}

string CStudentApp::GetCameraSName(int nDBCameraID)
{
	string strCameraName;
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	if (itorItem == m_MapNodeCamera.end())
		return strCameraName;
	GM_MapData & theMapData = itorItem->second;
	return theMapData["camera_name"];
}

QString CStudentApp::GetCameraQName(int nDBCameraID)
{
	QString strQCameraName = QTStr("Camera.Window.None");
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	if (itorItem == m_MapNodeCamera.end())
		return strQCameraName;
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	GM_MapData & theMapData = itorItem->second;
	string & strUTF8Name = theMapData["camera_name"];
	os_utf8_to_wcs(strUTF8Name.c_str(), strUTF8Name.size(), szWBuffer, MAX_PATH);
	return QString((QChar*)szWBuffer);
}

QString CStudentApp::GetCameraPullUrl(int nDBCameraID)
{
	QString strPullUrl = QTStr("Camera.Window.None");
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	do {
		if (itorItem == m_MapNodeCamera.end())
			break;
		GM_MapData & theMapData = itorItem->second;
		string & strStreamProp = theMapData["stream_prop"];
		if (strStreamProp.size() <= 0)
			break;
		WCHAR szWBuffer[MAX_PATH] = { 0 };
		STREAM_PROP nStreamType = (STREAM_PROP)atoi(strStreamProp.c_str());
		string & strStreamMP4 = theMapData["stream_mp4"];
		string & strStreamUrl = theMapData["stream_url"];
		if (nStreamType == kStreamMP4File) {
			os_utf8_to_wcs(strStreamMP4.c_str(), strStreamMP4.size(), szWBuffer, MAX_PATH);
			strPullUrl = QString((QChar*)szWBuffer);
		} else if(nStreamType == kStreamUrlLink) {
			os_utf8_to_wcs(strStreamUrl.c_str(), strStreamUrl.size(), szWBuffer, MAX_PATH);
			strPullUrl = QString((QChar*)szWBuffer);
		}
	} while (false);
	return strPullUrl;
}

string CStudentApp::UTF8_ANSI(const char * lpUValue)
{
	string strSValue;
	USES_CONVERSION;
	if (lpUValue == NULL || strlen(lpUValue) <= 0)
		return strSValue;				// 验证有效性
	_acp = CP_UTF8;                     // 设置code page
	LPCWSTR lpWValue = A2W(lpUValue);	// UTF-8 to Unicode Wide char
	_acp = CP_ACP;                      // 设置code page
	strSValue = W2A(lpWValue);			// Unicode Wide char to ANSI
	return strSValue;
}

string CStudentApp::ANSI_UTF8(const char * lpSValue)
{
	string strUValue;
	USES_CONVERSION;
	if (lpSValue == NULL || strlen(lpSValue) <= 0)
		return strUValue;				// 验证有效性
	_acp = CP_ACP;                      // 设置code page(默认)

	// 2009/09/01 A2W 在某些系统下会发生崩溃...
	// LPCWSTR lpWValue = A2W(lpSValue); // ANSI to Unicode Wide char
	int dwSize = MultiByteToWideChar(_acp, 0, lpSValue, -1, NULL, 0);
	wchar_t * lpWValue = new wchar_t[dwSize];
	MultiByteToWideChar(_acp, 0, lpSValue, -1, lpWValue, dwSize);

	_acp = CP_UTF8;                     // 设置code page, 得到编码长度, Unicode Wide char to UTF-8
	int ret = WideCharToMultiByte(_acp, 0, lpWValue, -1, NULL, NULL, NULL, NULL);
	strUValue.resize(ret); ASSERT(ret == strUValue.size());

	ret = WideCharToMultiByte(_acp, 0, lpWValue, -1, (LPSTR)strUValue.c_str(), strUValue.size(), NULL, NULL);
	ASSERT(ret == strUValue.size());

	delete[] lpWValue;

	return strUValue;
}

enum audio_format CStudentApp::convert_sample_format(int f)
{
	switch (f) {
	case AV_SAMPLE_FMT_U8:   return AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_S16:  return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:  return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_FLT:  return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:  return AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P: return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P: return AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP: return AUDIO_FORMAT_FLOAT_PLANAR;
	default:;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

enum speaker_layout CStudentApp::convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:     return SPEAKERS_UNKNOWN;
	case 1:     return SPEAKERS_MONO;
	case 2:     return SPEAKERS_STEREO;
	case 3:     return SPEAKERS_2POINT1;
	case 4:     return SPEAKERS_4POINT0;
	case 5:     return SPEAKERS_4POINT1;
	case 6:     return SPEAKERS_5POINT1;
	case 8:     return SPEAKERS_7POINT1;
	default:    return SPEAKERS_UNKNOWN;
	}
}
