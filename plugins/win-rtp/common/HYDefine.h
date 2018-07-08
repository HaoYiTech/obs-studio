
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
#include <util/platform.h>
#include <util/circlebuf.h>

#define LOCAL_ADDRESS_V4			"127.0.0.1"				// 本地地址定义-IPV4
#define LINGER_TIME					500						// SOCKET停止时数据链路层BUFF清空的最大延迟时间

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
