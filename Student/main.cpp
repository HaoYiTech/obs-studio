
#include "student-app.h"
#include "qt-wrappers.hpp"

static log_handler_t def_log_handler;

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	get_last_log(false, "student/logs", lastLogFile);

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "student/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath,	ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path, ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "student/logs");
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
	CStudentApp program(argc, argv);
	try {
		// ��ʼ��Ӧ�ó���...
		program.AppInit();
		// ÿ�����������µ���־�ļ�...
		create_log_file(logFile);
		// ��ʼ����¼����...
		program.doLoginInit();
		// ִ����ѭ������...
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
	// ���ô�����־�Ĵ�����...
	base_get_log_handler(&def_log_handler, nullptr);

	fstream logFile;
	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());

	// �������ô����ӡ����������޷���ӡ...
	base_set_log_handler(nullptr, nullptr);
	return ret;
}
