
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
		prof_release(static_cast<void*>(&ProfilerFree),	ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{ run_program_init };
	QCoreApplication::addLibraryPath(".");
	CStudentApp program(argc, argv, profilerNameStore.get());

	try {
		// ��ʼ��Ӧ�ó���...
		program.AppInit();
		// �������ַ������...
		OBSTranslator translator;
		program.installTranslator(&translator);
		// �жϽ����Ƿ��ظ����� => ֻ������һ������...
		bool already_running = false;
		// ������뱣�ַ���ֵrom����Ч�ԣ���Ч����֤�������Ѿ�����...
		RunOnceMutex rom = GetRunOnceMutex(already_running);
		// ��������Ѿ�������������ʾ���˳�...
		if (already_running) {
			OBSMessageBox::information(nullptr, 
				QTStr("AlreadyRunning.Title"),
				QTStr("AlreadyRunning.Text"));
			return 0;
		}
		// ÿ�����������µ���־�ļ�...
		create_log_file(logFile);
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
		// ��ʼ����¼����...
		program.doLoginInit();
		prof.Stop();
		// ִ����ѭ������...
		return program.exec();
	} catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
		OBSErrorBox(nullptr, "%s", error);
	}
	return ret;
}

/*// ���������ͽ���Ĺ�������...
#define MAX_SAMPLERATE   (16000)
#define MAX_SAMPLES_10MS ((MAX_SAMPLERATE*10)/1000)

// ����webrtc��aec���� => float
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
		// ���ò�ȷ����ʱ���Զ����벨�� => �����ڳ�ʼ��֮ǰ����...
		WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
		WebRtcAec_enable_extended_filter(lpAEC->aec, true);
		// ��ʼ��������������ʧ�ܣ�ֱ�ӷ���...
		if (WebRtcAec_Init(hAEC, nSampleRate, nSampleRate) != 0)
			break;
		// �򿪻򴴽����PCM�ļ����...
		lpMicFile = fopen(pMicFName, "rb");
		lpRefFile = fopen(pRefFName, "rb");
		lpAecFile = fopen(pAecFName, "wb");
		if (lpMicFile == NULL || lpRefFile == NULL || lpAecFile == NULL)
			break;
		// ��ʼ��PCM�ļ���ȡ���ݣ������л�����������...
		while (true) {
			if (fread(smic, sizeof(short), nFrameSamples, lpMicFile) != nFrameSamples)
				break;
			if (fread(sref, sizeof(short), nFrameSamples, lpRefFile) != nFrameSamples)
				break;
			// ��Ҫ��short��ʽת����float��ʽ...
			for (int i = 0; i < nFrameSamples; i++)	{
				dref[i] = (float)sref[i];
				dmic[i] = (float)smic[i];
			}
			float* const pfmic = &(dmic[0]);
			const float* const* ppfmic = &pfmic;
			float* const pfaec = &(daec[0]);
			float* const* ppfaec = &pfaec;
			// ע�⣺ʹ���Զ�������ʱģʽ�����Խ�msInSndCardBuf��������Ϊ0...
			// �Ƚ����������ݽ���Զ��Ͷ�ݣ���Ͷ����˷����ݽ��л������� => Ͷ����������...
			err_code = WebRtcAec_BufferFarend(hAEC, dref, nFrameSamples);
			err_code = WebRtcAec_Process(hAEC, ppfmic, 1, ppfaec, nFrameSamples, 0, 0);
			// ��float���ݸ�ʽת����short���ݸ�ʽ...
			for (int i = 0; i < nFrameSamples; i++) {
				saec[i] = (short)daec[i];
			}
			// ����������ϣ����д��̲���...
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

// ����webrtc��ns���� => short
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
	// ���뺯����Ŀǰֻ����short���ݸ�ʽ��������float���ݸ�ʽ...
	do {
		// ����һ��short��ʽ�Ľ������...
		if ((nsInst = WebRtcNsx_Create()) == NULL)
			break;
		// ��ʼ���������ʹ��16kƵ��...
		if (WebRtcNsx_Init(nsInst, nSampleRate) != 0)
			break;
		// �趨����̶� => 0: Mild, 1: Medium , 2: Aggressive
		if (WebRtcNsx_set_policy(nsInst, 2) != 0)
			break;
		// ��ԭʼ��Ƶ�ļ��������������ļ�...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// �����������ļ��������������ļ�...
		if ((lpOutPCM = fopen(lpOutFName, "wb")) == NULL)
			break;
		while (true) {
			// ��ȡָ�����ȵ�������Ƶ�ļ����� => 10ms���ݳ���...
			if (fread(sInFrame, sizeof(short), nFrameSamples, lpInPCM) != nFrameSamples)
				break;
			short* const pInData = &(sInFrame[0]);
			const short* const* ppInData = &pInData;
			short* const pOutData = &(sOutFrame[0]);
			short* const* ppOutData = &pOutData;
			// ���봦��Ȼ��ֱ��д���ļ�����...
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

/*// ����MD5�ļ����������...
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

// ������Ƶ�ٶȵ��������...

/*// ������ƵƵ��ת���Ĳ���...
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
		// ��ԭʼ��Ƶ�ļ�...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// �������������ļ�...
		if ((lpOutPCM = fopen(szOutFName, "wb")) == NULL)
			break;
		while (true) {
			// ��ȡָ�����ȵ�������Ƶ�ļ�����...
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

/*// ������Ƶ�ٶȵ��������...
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
		// ��ԭʼ��Ƶ�ļ�...
		if ((lpInPCM = fopen(lpInFName, "rb")) == NULL)
			break;
		// �������������ļ�...
		if ((lpOutPCM = fopen(szOutFName, "wb")) == NULL)
			break;
		float fTotalTime = 0.0f;
		int   samplesReaded = 0;
		int   samplesWritten = 0;
		while (true) {
			// ��ȡָ�����ȵ�������Ƶ�ļ�����...
			samplesReaded = fread(sInFrame, sizeof(short), input_frames, lpInPCM);
			if (samplesReaded <= 0) {
				// ��ȡ��ĩβ��ˢ�²���...
				sonicFlushStream(stream);
				// ��ȡ���Ĳ�����������...
				samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
				if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
				fTotalTime += samplesWritten / (sampleRate * 1.0f);
				break;
			}
			// ����X�������ٻָ�X������Ȼ���ټ���X����������ѭ��...
			if (nState == 0) {
				if (nStepNum++ >= JUMP_NUM) {
					// ���ٵ���...
					speed = 1.0;
					// ���ٷ����仯ʱ��ˢ�²��� => ���ڻ����հ����ݣ���Ҫ����ʹ��...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// �趨�µ�״̬�ͱ���...
					nState = 1; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			} else if (nState == 1) {
				if (nStepNum++ >= JUMP_NUM) {
					// ���ٵ���...
					speed = 0.5;
					// ���ٷ����仯ʱ��ˢ�²��� => ���ڻ����հ����ݣ���Ҫ����ʹ��...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// �趨�µ�״̬�ͱ���...
					nState = 2; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			} else if (nState == 2) {
				if (nStepNum++ >= JUMP_NUM) {
					// ���ٵ���...
					speed = 2.0;
					// ���ٷ����仯ʱ��ˢ�²��� => ���ڻ����հ����ݣ���Ҫ����ʹ��...
					//if (sonicGetSpeed(stream) != speed) {
					//	sonicFlushStream(stream);
					//	samplesWritten = sonicReadShortFromStream(stream, sOutFrame, FRAME_SAMPLES * 3);
					//	if (samplesWritten > 0) { fwrite(sOutFrame, sizeof(short), samplesWritten, lpOutPCM); fflush(lpOutPCM); }
					//}
					// �趨�µ�״̬�ͱ���...
					nState = 0; nStepNum = 0;
					sonicSetSpeed(stream, speed);
				}
			}
			// д���µ�ԭʼ��Ƶ���ݣ����뱣֤����ͬ�ٶ���û�������ٶȲ���...
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

int main(int argc, char *argv[])
{
	// ����webrtc��aed����...
	//return doTestWebrtcAEC();
	// ����webrtc��ns����...
	//return doTestWebrtcNS();
	// ����MD5�ļ����������...
	//if ((argc > 1 && stricmp(argv[1], "-upload") == 0) ||
	//	(argc > 2 && stricmp(argv[2], "-upload") == 0) )
	//	return doTestMd5();
	// ������ƵƵ��ת���Ĳ���...
	//return doTestAudioConvert();
	// ������Ƶ�ٶȵ��������...
	//return doTestAudioSpeed();

	// ��ʼ�������׽���...
	WSADATA	wsData = { 0 };
	WORD	wsVersion = MAKEWORD(2, 2);
	(void)::WSAStartup(wsVersion, &wsData);

	// ��ʼ���̺߳��׽���ͨ�ÿ�...
	OSThread::Initialize();
	SocketUtils::Initialize();

	// ���ô�����־�Ĵ�����...
	base_get_log_handler(&def_log_handler, nullptr);

	fstream logFile;
	curl_global_init(CURL_GLOBAL_ALL);
	int nRet = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// �������ô����ӡ����������޷���ӡ...
	base_set_log_handler(nullptr, nullptr);

	// �ͷ��̺߳��׽���ͨ�ÿ�...
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

// ע�⣺ͳһ����UTF8��ʽ...
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

// ע�⣺ͳһ����UTF8��ʽ...
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

// ע�⣺ͳһ����UTF8��ʽ...
char * CStudentApp::GetServerOS()
{
	static char szBuffer[MAX_PATH] = { 0 };
	if (strlen(szBuffer) > 0)
		return szBuffer;
	::sprintf(szBuffer, "%s", CStudentApp::GetSystemVer().c_str());
	assert(::strlen(szBuffer) <= MAX_PATH);
	return szBuffer;
}

// ע�⣺ͳһ����UTF8��ʽ...
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

// ����λ�ã���� run_program() ������ֻ����һ��...
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
	// ����Ĭ�ϵ���Ƶ���š���������ʣ����������...
	m_nAudioOutSampleRate = DEF_AUDIO_OUT_SAMPLE_RATE;
	m_nAudioOutChannelNum = DEF_AUDIO_OUT_CHANNEL_NUM;
	m_nAudioOutBitrateAAC = DEF_AUDIO_OUT_BITRATE_AAC;
}

CStudentApp::~CStudentApp()
{
	// ɾ��TCP�����Ӷ���...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession;
	}
	// ��վ�߳���Ч��ֱ��ɾ��...
	if (m_lpWebThread != NULL) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// �ͷ�Comϵͳ����...
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

// �����ͨ��Ͷ����������Ƶ��������...
void CStudentApp::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	if (m_studentWindow != NULL) {
		CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
		((lpViewLeft != NULL) ? lpViewLeft->doEchoCancel(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf) : NULL);
	}
}

// ֪ͨ��ര�ڣ����ղ����߳��Ѿ�ֹͣ��...
void CStudentApp::onUdpRecvThreadStop()
{
	if (m_studentWindow != NULL) {
		CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
		((lpViewLeft != NULL) ? lpViewLeft->onUdpRecvThreadStop() : NULL);
	}
}

bool CStudentApp::InitMacIPAddr()
{
	// 2018.01.11 - ��� ERROR_BUFFER_OVERFLOW ������...
	DWORD outBuflen = sizeof(IP_ADAPTER_INFO);
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO;
	DWORD retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	// ���ֻ����������ã����·���ռ�...
	if (retStatus == ERROR_BUFFER_OVERFLOW) {
		MsgLogGM(retStatus);
		delete lpAdapter; lpAdapter = NULL;
		lpAdapter = (PIP_ADAPTER_INFO)new BYTE[outBuflen];
		retStatus = GetAdaptersInfo(lpAdapter, &outBuflen);
	}
	// ���Ƿ����˴��󣬴�ӡ����ֱ�ӷ���...
	if (retStatus != ERROR_SUCCESS) {
		delete lpAdapter; lpAdapter = NULL;
		MsgLogGM(retStatus);
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
	// ��ȡĬ�ϵ�����·��...
	string defaultLangPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", defaultLangPath)) {
		OBSErrorBox(NULL, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}
	// �����ֵ����...
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
	// ��ȡȫ�������ļ���·�� => ��ʱĿ¼...
	if (GetConfigPath(path, sizeof(path), "obs-student/global.ini") <= 0 )
		return false;
	// ��ȫ�������ļ�...
	int errorcode = m_globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}
	// �����ļ���û���������ò���������Ӳ������������ò���...
	if (!config_has_user_value(m_globalConfig, "General", "WebCenter")) {
		config_set_string(m_globalConfig, "General", "WebCenter", DEF_WEB_CENTER);
		changed = true;
	}
	// �������ļ����ж�ȡ������վ�ĵ�ַ => https://
	m_strWebCenter = config_get_string(m_globalConfig, "General", "WebCenter");
	// �鿴�ڵ���վ��ַ�Ƿ���Ч...
	if (!config_has_user_value(m_globalConfig, "General", "WebClass")) {
		config_set_string(m_globalConfig, "General", "WebClass", DEF_WEB_CLASS);
		changed = true;
	}
	// �������ļ����ж�ȡ�ƽ�����վ�ĵ�ַ => https://
	m_strWebClass = config_get_string(m_globalConfig, "General", "WebClass");
	// �鿴�����ļ����Ƿ��б���AEC�������ݱ�־�����ֶ�...
	if (!config_has_user_value(m_globalConfig, "General", "SaveAECSample")) {
		config_set_bool(m_globalConfig, "General", "SaveAECSample", false);
		changed = true;
	}
	// ��ȫ�������ļ��ж�ȡ�Ƿ񱣴���Ƶ����������������...
	m_bSaveAECSample = config_get_bool(m_globalConfig, "General", "SaveAECSample");
	// �����б仯�����̵�global.ini�����ļ�����...
	if (changed) {
		config_save_safe(m_globalConfig, "tmp", nullptr);
	}
	// ����Ĭ�ϵĳ�ʼ������Ϣ...
	return InitGlobalConfigDefaults();
}

bool CStudentApp::InitGlobalConfigDefaults()
{
	// ����Ĭ��������Ϣ�������Զ��������������Ϣ...
	config_set_default_string(m_globalConfig, "General", "Language", DEFAULT_LANG);
	config_set_default_uint(m_globalConfig, "General", "EnableAutoUpdates", true);
	config_set_default_uint(m_globalConfig, "General", "MaxLogs", 10);
	return true;
}

// ��������ʼ����¼����...
void CStudentApp::doLoginInit()
{
	// ������¼���ڣ���ʾ��¼����...
	//m_RoomWindow = new LoginWindow();
	//m_RoomWindow->show();
	// ������¼������Ӧ�ö�����źŲ۹�������...
	//connect(m_RoomWindow, SIGNAL(doTriggerLoginSuccess(string&)), this, SLOT(onTriggerLoginSuccess(string&)));

	// ����С�����ά���¼����...
	m_LoginMini = new CLoginMini();
	m_LoginMini->show();
	// ������¼������Ӧ�ö�����źŲ۹�������...
	connect(m_LoginMini, SIGNAL(doTriggerMiniSuccess()), this, SLOT(onTriggerMiniSuccess()));
	// ���������źŲ۷�������¼�...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
}

// ����С�����¼�ɹ�֮����źŲ��¼�...
void CStudentApp::onTriggerMiniSuccess()
{
	// �����¼�����...
	int nDBRoomID = m_LoginMini->GetDBRoomID();
	m_nDBUserID = m_LoginMini->GetDBUserID();
	m_strRoomID = QString("%1").arg(nDBRoomID).toStdString();
	// �ȹر�С�����¼����...
	m_LoginMini->close();
	// �������������ڽ���...
	setQuitOnLastWindowClosed(false);
	m_studentWindow = new StudentWindow();
	m_studentWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	this->connect(m_studentWindow, SIGNAL(destroyed()), this, SLOT(quit()));
	m_studentWindow->InitWindow();
	// �����źŲ۹������� => ���ڸ����߳̽����¼�֪ͨ...
	this->connect(this, SIGNAL(msgFromWebThread(int, int, int)), m_studentWindow, SLOT(doWebThreadMsg(int, int, int)));
	this->connect(this, SIGNAL(doEnableCamera(OBSQTDisplay*)), m_studentWindow->GetViewLeft(), SLOT(doEnableCamera(OBSQTDisplay*)));
	// ����������һ����վ�����߳�...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
}

// �������¼�ɹ�֮����źŲ��¼�...
/*void CStudentApp::onTriggerLoginSuccess(string & strRoomID)
{
	// �����¼�����...
	m_strRoomID = strRoomID;
	// �رշ����¼����...
	m_RoomWindow->close();
	// �������������ڽ���...
	setQuitOnLastWindowClosed(false);
	m_studentWindow = new StudentWindow();
	m_studentWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	this->connect(m_studentWindow, SIGNAL(destroyed()), this, SLOT(quit()));
	m_studentWindow->InitWindow();
	// �����źŲ۹������� => ���ڸ����߳̽����¼�֪ͨ...
	this->connect(this, SIGNAL(msgFromWebThread(int, int, int)), m_studentWindow, SLOT(doWebThreadMsg(int, int, int)));
	this->connect(this, SIGNAL(doEnableCamera(OBSQTDisplay*)), m_studentWindow->GetViewLeft(), SLOT(doEnableCamera(OBSQTDisplay*)));
	// ����������һ����վ�����߳�...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
}*/

// ��¼�ɹ���ȡ�ն�����֮������������ת������...
void CStudentApp::onWebLoadResource()
{
	// ����Զ����ת����������...
	this->doCheckRemote();
	// ����һ����ʱ���ʱ�� => ÿ��5��ִ��һ��...
	m_nFastTimer = this->startTimer(5 * 1000);
	// ÿ��15����һ�Σ��ӷ�������ȡ����ͳ�Ʋ��������ݿ�...
	m_nFlowTimer = this->startTimer(15 * 1000);
	// ÿ��30����һ�Σ�ѧ��������ת������������ͨ���б�...
	m_nOnLineTimer = this->startTimer(30 * 1000);
}

// ����ѧ������Ȩ���ڵ����...
void CStudentApp::onWebAuthExpired()
{
	this->doLogoutEvent();
}

// ʱ�Ӷ�ʱִ�й���...
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

// ��Ӧȫ�ֵļ����¼���ר�Ŵ��������ķŴ���С����...
bool CStudentApp::notify(QObject * inObject, QEvent * inEvent)
{
	// �Լ��̰����¼�����Ԥ�������...
	if (inEvent->type() == QEvent::KeyPress) {
		QKeyEvent * keyEvent = static_cast<QKeyEvent*>(inEvent);
		int nKeyItem = keyEvent->key();
		// ��������Ч�ǲŴ���ר�Ź��������ķŴ����С�¼�����ȷ��Ӧ�����ٴ���...
		if ((m_studentWindow != NULL) && m_studentWindow->VolumeEvent(inObject, nKeyItem))
			return true;
		// û����ȷ��Ӧ�����¼�����Ҫ�������ݣ����������Ӧ...
	}
	return QApplication::notify(inObject, inEvent);
}

void CStudentApp::doCheckFDFS()
{
	this->doCheckRemote();
}

void CStudentApp::doCheckRemote()
{
	// �ж�Զ����ת��������ַ�Ƿ���Ч...
	if (m_strRemoteAddr.size() <= 0 || m_nRemotePort <= 0)
		return;
	// ���Զ�̻Ự�Ѿ����ڣ������Ѿ����ӣ�ֱ�ӷ���...
	if (m_RemoteSession != NULL && !m_RemoteSession->IsCanReBuild())
		return;
	// ����Ự��Ч����ɾ��֮...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// ��ʼ��Զ����ת�Ự����...
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(m_strRemoteAddr.c_str(), m_nRemotePort);
	// ��Զ�̻Ự���źŲ��������ڶ�������໥�������� => UDP���ӶϿ�ʱ���¼�֪ͨ...
	connect(m_RemoteSession, SIGNAL(doTriggerUdpLogout(int, int, int)), m_studentWindow, SLOT(onTriggerUdpLogout(int, int, int)));
	connect(m_RemoteSession, SIGNAL(doTriggerCameraPTZ(int, int, int)), m_studentWindow, SLOT(onTriggerCameraPTZ(int, int, int)));
	// ������ര���뷢���߳��¼������ӷ������ɹ����źŲ�...
	CViewLeft * lpViewLeft = m_studentWindow->GetViewLeft();
	connect(m_RemoteSession, SIGNAL(doTriggerConnected()), lpViewLeft, SLOT(onTriggerConnected()));
	connect(m_RemoteSession, SIGNAL(doTriggerLiveStop(int)), lpViewLeft, SLOT(onTriggerLiveStop(int)));
	connect(m_RemoteSession, SIGNAL(doTriggerLiveStart(int)), lpViewLeft, SLOT(onTriggerLiveStart(int)));
	// ����Զ�̻Ự����ʦ���ڶ�����źŲ۹�������...
	CViewRight * lpViewRight = m_studentWindow->GetViewRight();
	CViewTeacher * lpViewTeacher = lpViewRight->GetViewTeacher();
	// ��Զ�̻Ự���źŲ۽����໥���� => ��ʦ���������߻�����ʱ���¼�֪ͨ...
	if (lpViewTeacher != NULL) {
		connect(m_RemoteSession, SIGNAL(doTriggerRecvThread(bool)), lpViewTeacher, SLOT(onTriggerUdpRecvThread(bool)));
		connect(m_RemoteSession, SIGNAL(doTriggerDeleteExAudioThread()), lpViewTeacher, SLOT(onTriggerDeleteExAudioThread()));
	}
}

// ͨ��Webת��ͳ���ѵ�¼��������...
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
	// �������������󣬴�ӡ������Ϣ������ѭ��...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		return;
	}
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	//blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
}

// ��������Ƿ�����������״̬...
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

// �ؽ�ϵͳ��Դ...
void CStudentApp::doReBuildResource()
{
	// �˳���ɾ�������Դ...
	this->doLogoutEvent();
	// ����������һ����վ�����߳�...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread();
	theErr = m_lpWebThread->InitThread();
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// ����һ����ʱ���ʱ�� => ÿ��5��ִ��һ��...
	m_nFastTimer = this->startTimer(5 * 1000);
	// ÿ��30����һ�Σ�ѧ��������ת������������ͨ���б�...
	m_nOnLineTimer = this->startTimer(30 * 1000);
}
//
// ����ѧ�����˳��¼�֪ͨ...
void CStudentApp::doLogoutEvent()
{
	// ɾ�����ʱ��...
	(m_nFastTimer >= 0) ? this->killTimer(m_nFastTimer) : NULL;
	(m_nOnLineTimer >= 0) ? this->killTimer(m_nOnLineTimer) : NULL;
	m_nOnLineTimer = m_nFastTimer = -1;
	// ɾ��TCP�����Ӷ���...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession;
	}
	// ֪ͨ��վѧ�����˳�...
	if (m_lpWebThread != NULL) {
		m_lpWebThread->doWebGatherLogout();
	}
	// ��վ�߳���Ч��ֱ��ɾ��...
	if (m_lpWebThread != NULL) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// ֱ�����ý��㴰�ڶ���...
	m_lpFocusDisplay = NULL;
	// ����ͨ�����ü��϶���...
	m_MapNodeCamera.clear();
}

// ����ͷ���ڱ�ɾ��ʱ�ᱻ���� => ����Լ������Ǹ����㣬��Ҫ����...
void CStudentApp::doResetFocus(OBSQTDisplay * lpCurDisplay)
{
	// �������ָ����Ч���뵱ǰ�������һ�£�ֱ�ӷ���...
	if ((lpCurDisplay == NULL) || (m_lpFocusDisplay != lpCurDisplay))
		return;
	// ���ý��㴰�ڶ���...
	m_lpFocusDisplay = NULL;
}

void CStudentApp::doSaveFocus(OBSQTDisplay * lpNewDisplay)
{
	// ���ָ����Ч����ͬ��ֱ�ӷ���...
	if ((lpNewDisplay == NULL) || (m_lpFocusDisplay == lpNewDisplay))
		return;
	// ֮ǰ�Ľ�����Ч���ͷŽ���...
	if (m_lpFocusDisplay != NULL) {
		m_lpFocusDisplay->doReleaseFocus();
	}
	// ����Ϊ�µĽ������...
	m_lpFocusDisplay = lpNewDisplay;
	// ����½�����CViewCamera����...
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
		return strSValue;				// ��֤��Ч��
	_acp = CP_UTF8;                     // ����code page
	LPCWSTR lpWValue = A2W(lpUValue);	// UTF-8 to Unicode Wide char
	_acp = CP_ACP;                      // ����code page
	strSValue = W2A(lpWValue);			// Unicode Wide char to ANSI
	return strSValue;
}

string CStudentApp::ANSI_UTF8(const char * lpSValue)
{
	string strUValue;
	USES_CONVERSION;
	if (lpSValue == NULL || strlen(lpSValue) <= 0)
		return strUValue;				// ��֤��Ч��
	_acp = CP_ACP;                      // ����code page(Ĭ��)

	// 2009/09/01 A2W ��ĳЩϵͳ�»ᷢ������...
	// LPCWSTR lpWValue = A2W(lpSValue); // ANSI to Unicode Wide char
	int dwSize = MultiByteToWideChar(_acp, 0, lpSValue, -1, NULL, 0);
	wchar_t * lpWValue = new wchar_t[dwSize];
	MultiByteToWideChar(_acp, 0, lpSValue, -1, lpWValue, dwSize);

	_acp = CP_UTF8;                     // ����code page, �õ����볤��, Unicode Wide char to UTF-8
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
