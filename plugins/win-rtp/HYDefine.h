
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
#include "../../Student/common/rtp.h"

//#define DEBUG_FRAME                          // RTP�Ƿ��������֡...
//#define DEBUG_DECODE                         // RTP�Ƿ���Խ�����...
//#define DEBUG_SEND_ONLY                      // RTP�Ƿ�ֻ���������߳�...
//#define DEBUG_RECV_ONLY                      // RTP�Ƿ�ֻ�����ۿ��߳�...

#define TM_RECV_NAME       "[Teacher-Looker]"  // ��ʦ�ۿ���...
#define TM_SEND_NAME       "[Teacher-Pusher]"  // ��ʦ������...

#define LOCAL_ADDRESS_V4   "127.0.0.1"         // ���ص�ַ����-IPV4
#define LINGER_TIME         500                // SOCKETֹͣʱ������·��BUFF��յ�����ӳ�ʱ��
#define DEF_MTU_SIZE        800                // Ĭ��MTU��Ƭ��С(�ֽ�)...
#define LOG_MAX_SIZE        2048               // ������־��󳤶�(�ֽ�)...
#define MAX_BUFF_LEN        1024               // ����ĳ���(�ֽ�)...
#define MAX_SLEEP_MS		15				   // �����Ϣʱ��(����)
#define ADTS_HEADER_SIZE	7				   // AAC��Ƶ���ݰ�ͷ����(�ֽ�)...
#define DEF_CIRCLE_SIZE		128	* 1024		   // Ĭ�ϻ��ζ��г���(�ֽ�)...
#define DEF_TIMEOUT_MS		5 * 1000		   // Ĭ�ϳ�ʱʱ��(����)...

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
