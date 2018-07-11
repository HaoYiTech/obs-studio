
#include "OSThread.h"
#include "SocketUtils.h"
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-rtp", "en-US")


void InitModule();
void UnInitModule();
void RegisterWinRtpSource();
void RegisterWinRtpOutput();

bool obs_module_load(void)
{
	// ��ʼ��ģ��...
	InitModule();
	// ע������Դ�����...
	RegisterWinRtpSource();
	RegisterWinRtpOutput();
	return true;
}

void obs_module_unload(void)
{
	UnInitModule();
}

void InitModule()
{
	// ��ʼ���׽���...
	WORD	wsVersion = MAKEWORD(2, 2);
	WSADATA	wsData = { 0 };
	(void)::WSAStartup(wsVersion, &wsData);
	// ��ʼ�����硢�߳�...
	OSThread::Initialize();
	SocketUtils::Initialize();
}

void UnInitModule()
{
	// �ͷŷ����ϵͳ��Դ...
	SocketUtils::UnInitialize();
	OSThread::UnInitialize();
}