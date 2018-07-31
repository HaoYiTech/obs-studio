
#pragma once

#include <assert.h>

#ifndef ASSERT
#define ASSERT assert 
#endif // ASSERT

#define DEF_CLOUD_CLASS				"http://edu.ihaoyi.cn"		// 云教室地址
#define DEF_WEB_HOME				"https://www.myhaoyi.com"	// 默认中心网站 => 必须是 https:// 兼容小程序接口...
#define DEF_WEB_PORT				80							// Web默认端口 

enum AUTH_STATE
{
	kAuthRegister = 1,		// 网站注册授权
	kAuthExpired = 2,		// 授权过期验证
};

enum STREAM_PROP
{
	kStreamDevice = 0,		// 摄像头设备流类型 => camera
	kStreamMP4File = 1,		// MP4文件流类型    => .mp4
	kStreamUrlLink = 2,		// URL链接流类型    => rtsp/rtmp
};

enum CAMERA_STATE
{
	kCameraWait = 0,		// 未连接
	kCameraRun = 1,			// 运行中
	kCameraRec = 2,			// 录像中
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
