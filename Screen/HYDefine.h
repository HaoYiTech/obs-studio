
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

//#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
