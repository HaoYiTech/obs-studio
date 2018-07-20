
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

using namespace std;

#include "obs.h"
#include "GMError.h"
#include "HYVersion.h"
#include <util/platform.h>
#include <util/circlebuf.h>

#define DEF_CAMERA_START_ID			1							// 默认摄像头开始ID
#define DEF_MAX_CAMERA              8							// 默认最大摄像头数目
#define DEF_MAIN_NAME				"采集端"						// 默认主窗口标题 => 主进程是Unicode，这里自动就会成为Unicode格式...
#define DEF_WEB_HOME				"https://www.myhaoyi.com"	// 默认中心网站 => 必须是 https:// 兼容小程序接口...
#define DEF_WEB_PORT				80							// Web默认端口 
#define DEF_CLOUD_CLASS				"http://edu.ihaoyi.cn"		// 云教室地址
#define LINGER_TIME					500							// SOCKET停止时数据链路层BUFF清空的最大延迟时间

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
