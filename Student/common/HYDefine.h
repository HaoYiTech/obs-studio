
#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

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
#include <deque>
#include <string>
#include <functional>

using namespace std;

#include "obs.h"
#include "rtp.h"
#include "GMError.h"
#include "HYCommon.h"
#include "HYVersion.h"
#include <util/platform.h>
#include <util/circlebuf.h>

//#define DEBUG_AEC	                                      // 是否调试AEC...
//#define DEBUG_FRAME                                     // RTP是否调试数据帧...
//#define DEBUG_DECODE                                    // RTP是否调试解码器...
//#define DEBUG_SEND_ONLY                                 // RTP是否只启动推流线程...
//#define DEBUG_RECV_ONLY                                 // RTP是否只启动观看线程...

#define TM_RECV_NAME               "[Student-Looker]"     // 学生观看端...
#define TM_SEND_NAME               "[Student-Pusher]"     // 学生推流端...

#define DEF_MTU_SIZE                800                   // 默认MTU分片大小(字节)...
#define LOG_MAX_SIZE                2048                  // 单条日志最大长度(字节)...
#define MAX_BUFF_LEN                1024                  // 最大报文长度(字节)...
#define MAX_SLEEP_MS                15                    // 最大休息时间(毫秒)
#define ADTS_HEADER_SIZE            7                     // AAC音频数据包头长度(字节)...
#define DEF_CIRCLE_SIZE	            128	* 1024            // 默认环形队列长度(字节)...
#define MAX_SEND_JAM_MS             4000                  // 最大发送拥塞缓存时间(毫秒)...
#define MAX_MULTI_JAM_MS            4000                  // 最大组播拥塞缓存时间(毫秒)...

#define DEF_MULTI_DATA_ADDR         "234.5.6.7"           // 组播音视频数据转发地址...
#define DEF_MULTI_LOSE_ADDR         "235.6.7.8"           // 组播丢包重传转发地址...
														  
#define DEF_SSL_PROTO               "https"               // 默认协议头
#define DEF_WEB_PROTO               "http"                // 默认协议头
#define DEF_SSL_PORT                443                   // 默认https端口
#define DEF_WEB_PORT                80                    // 默认http 端口
														  
#define DEF_AUDIO_OUT_CHANNEL_NUM   1                     // 默认音频播放、压缩声道数
#define DEF_AUDIO_OUT_SAMPLE_RATE   16000                 // 默认音频播放、压缩采样率
#define DEF_AUDIO_OUT_BITRATE_AAC   32000                 // 默认回音消除后AAC压缩输出码流
#define DEF_WEBRTC_AEC_NN           160                   // 回音消除单次样本数(单个声道) => 采样率是16000时，刚好是10毫秒...
#define DEF_CAMERA_START_ID			1                     // 默认摄像头开始ID
#define DEF_MAX_CAMERA              8                     // 默认最大摄像头数目
#define LINGER_TIME					500	                  // SOCKET停止时数据链路层BUFF清空的最大延迟时间

#define WM_WEB_LOAD_RESOURCE		(WM_USER + 108)
#define	WM_WEB_AUTH_RESULT			(WM_USER + 110)

typedef	map<string, string>			GM_MapData;
typedef map<int, GM_MapData>		GM_MapNodeCamera;     // int  => 是指数据库DBCameraID

typedef struct
{
	string		strData;			// 帧数据
	int			typeFlvTag;			// FLV_TAG_TYPE_AUDIO or FLV_TAG_TYPE_VIDEO
	bool		is_keyframe;		// 是否是关键帧
	uint32_t	dwSendTime;			// 发送时间(毫秒)
	uint32_t	dwRenderOffset;		// 时间戳偏移值
}FMS_FRAME;

enum ROLE_TYPE
{
	kRoleWanRecv   = 0,      // 外网接收者角色
	kRoleMultiRecv = 1,      // 组播接收者角色
	kRoleMultiSend = 2,      // 组播发送者角色
};

enum CAMERA_STATE
{
	kCameraOffLine = 0,		// 未连接
	kCameraConnect = 1,		// 正在连接
	kCameraOnLine  = 2,		// 已连接
	kCameraPusher  = 3,     // 推流中
};

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
