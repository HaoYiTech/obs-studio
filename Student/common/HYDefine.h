
#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <assert.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef ASSERT
#define ASSERT assert 
#endif // ASSERT

typedef unsigned char		UInt8;
typedef signed char			SInt8;
typedef unsigned short		UInt16;
typedef signed short		SInt16;
typedef unsigned long		UInt32;
typedef signed long			SInt32;
typedef LONGLONG			SInt64;
typedef ULONGLONG			UInt64;
typedef float				Float32;
typedef double				Float64;
typedef UInt16				Bool16;

#pragma warning(disable: 4786)

#include <map>
#include <string>
#include <functional>

using namespace std;

#include "obs.h"
#include "rtp.h"
#include "GMError.h"
#include "HYVersion.h"
#include <util/platform.h>
#include <util/circlebuf.h>

#define DEF_AUDIO_OUT_CHANNEL_NUM   1                           // 默认音频播放、压缩声道数
#define DEF_AUDIO_OUT_SAMPLE_RATE   16000                       // 默认音频播放、压缩采样率
#define DEF_AUDIO_OUT_BITRATE_AAC   32000                       // 默认回音消除后AAC压缩输出码流
#define DEF_SPEEX_HORN_DELAY        0                           // 默认的扬声器延迟 => 采用WSAPI之后，没有扬声器延时了
#define DEF_SPEEX_FILTER_MS         200                         // 回音消除尾音长度 => 降低尾音长度，回音快速收敛...
#define DEF_SPEEX_FRAME_MS          16                          // 回音消除单次长度
#define DEF_CAMERA_START_ID			1							// 默认摄像头开始ID
#define DEF_MAX_CAMERA              8							// 默认最大摄像头数目
#define DEF_CLOUD_CLASS				"http://edu.ihaoyi.cn"		// 云教室地址
#define DEF_WEB_HOME				"https://www.myhaoyi.com"	// 默认中心网站 => 必须是 https:// 兼容小程序接口...
#define DEF_WEB_PORT				80							// Web默认端口 
#define LINGER_TIME					500							// SOCKET停止时数据链路层BUFF清空的最大延迟时间

#define WM_WEB_LOAD_RESOURCE		(WM_USER + 108)
#define	WM_WEB_AUTH_RESULT			(WM_USER + 110)

typedef	map<string, string>			GM_MapData;
typedef map<int, GM_MapData>		GM_MapNodeCamera;			// int  => 是指数据库DBCameraID

typedef struct
{
	string		strData;			// 帧数据
	int			typeFlvTag;			// FLV_TAG_TYPE_AUDIO or FLV_TAG_TYPE_VIDEO
	bool		is_keyframe;		// 是否是关键帧
	uint32_t	dwSendTime;			// 发送时间(毫秒)
	uint32_t	dwRenderOffset;		// 时间戳偏移值
}FMS_FRAME;

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
	kCameraOffLine = 0,		// 未连接
	kCameraConnect = 1,		// 正在连接
	kCameraOnLine  = 2,		// 已连接
};

// define client type...
enum {
	kClientPHP        = 1,
	kClientStudent    = 2,
	kClientTeacher    = 3,
	kClientUdpServer  = 4,
};

// define command id...
enum {
	kCmd_Student_Login		  = 1,
	kCmd_Student_OnLine		  = 2,
	kCmd_Teacher_Login		  = 3,
	kCmd_Teacher_OnLine		  = 4,
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

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
