
#pragma once

#include <assert.h>

#ifndef ASSERT
#define ASSERT assert 
#endif // ASSERT

#define DEF_WEB_CENTER				"https://www.myhaoyi.com"	// Ĭ��������վ(443) => ������ https:// ����С����ӿ�...
#define DEF_WEB_CLASS				"https://edu.ihaoyi.cn"		// Ĭ���ƽ�����վ(443) => ������ https:// ����С����ӿ�...

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
	kClientPHP         = 1,
	kClientStudent     = 2,
	kClientTeacher     = 3,
	kClientUdpServer   = 4,
};

// define command id...
enum {
	kCmd_Student_Login		  = 1,
	kCmd_Student_OnLine		  = 2,
	kCmd_Teacher_Login		  = 3,
	kCmd_Teacher_OnLine       = 4,
	kCmd_UDP_Logout			  = 5,
	kCmd_Camera_PullStart     = 6,
	kCmd_Camera_PullStop      = 7,
	kCmd_Camera_OnLineList    = 8,
	kCmd_Camera_LiveStart     = 9,
	kCmd_Camera_LiveStop      = 10,
	kCmd_UdpServer_Login      = 11,
	kCmd_UdpServer_OnLine     = 12,
	kCmd_UdpServer_AddTeacher = 13,
	kCmd_UdpServer_DelTeacher = 14,
	kCmd_UdpServer_AddStudent = 15,
	kCmd_UdpServer_DelStudent = 16,
	kCmd_PHP_GetUdpServer     = 17,
};

// define the command header...
typedef struct {
	int   m_pkg_len;    // body size...
	int   m_type;       // client type...
	int   m_cmd;        // command id...
	int   m_sock;       // php sock in transmit...
} Cmd_Header;
