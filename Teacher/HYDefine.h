
#pragma once

#include <assert.h>

#ifndef ASSERT
#define ASSERT assert 
#endif // ASSERT

#define DEF_CLOUD_CLASS				"http://edu.ihaoyi.cn"		// �ƽ��ҵ�ַ
#define DEF_WEB_HOME				"https://www.myhaoyi.com"	// Ĭ��������վ => ������ https:// ����С����ӿ�...
#define DEF_WEB_PORT				80							// WebĬ�϶˿� 

enum AUTH_STATE
{
	kAuthRegister = 1,		// ��վע����Ȩ
	kAuthExpired = 2,		// ��Ȩ������֤
};

enum STREAM_PROP
{
	kStreamDevice = 0,		// ����ͷ�豸������ => camera
	kStreamMP4File = 1,		// MP4�ļ�������    => .mp4
	kStreamUrlLink = 2,		// URL����������    => rtsp/rtmp
};

enum CAMERA_STATE
{
	kCameraWait = 0,		// δ����
	kCameraRun = 1,			// ������
	kCameraRec = 2,			// ¼����
};

// define client type...
enum {
	kClientPHP = 1,
	kClientStudent = 2,
	kClientTeacher = 3,
};

// define command id...
enum {
	kCmd_Student_Login		= 1,
	kCmd_Student_OnLine		= 2,
	kCmd_Teacher_Login		= 3,
	kCmd_Teacher_OnLine     = 4,
	kCmd_UDP_Logout			= 5,
};

// define the command header...
typedef struct {
	int   m_pkg_len;    // body size...
	int   m_type;       // client type...
	int   m_cmd;        // command id...
	int   m_sock;       // php sock in transmit...
} Cmd_Header;
