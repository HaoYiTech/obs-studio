
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

//#define DEBUG_AEC	                                      // �Ƿ����AEC...
//#define DEBUG_FRAME                                     // RTP�Ƿ��������֡...
//#define DEBUG_DECODE                                    // RTP�Ƿ���Խ�����...
//#define DEBUG_SEND_ONLY                                 // RTP�Ƿ�ֻ���������߳�...
//#define DEBUG_RECV_ONLY                                 // RTP�Ƿ�ֻ�����ۿ��߳�...

#define TM_RECV_NAME               "[Student-Looker]"     // ѧ���ۿ���...
#define TM_SEND_NAME               "[Student-Pusher]"     // ѧ��������...

#define DEF_MTU_SIZE                800                   // Ĭ��MTU��Ƭ��С(�ֽ�)...
#define LOG_MAX_SIZE                2048                  // ������־��󳤶�(�ֽ�)...
#define MAX_BUFF_LEN                1024                  // ����ĳ���(�ֽ�)...
#define MAX_SLEEP_MS                15                    // �����Ϣʱ��(����)
#define ADTS_HEADER_SIZE            7                     // AAC��Ƶ���ݰ�ͷ����(�ֽ�)...
#define DEF_CIRCLE_SIZE	            128	* 1024            // Ĭ�ϻ��ζ��г���(�ֽ�)...
#define MAX_SEND_JAM_MS             4000                  // �����ӵ������ʱ��(����)...
#define MAX_MULTI_JAM_MS            4000                  // ����鲥ӵ������ʱ��(����)...

#define DEF_MULTI_DATA_ADDR         "234.5.6.7"           // �鲥����Ƶ����ת����ַ...
#define DEF_MULTI_LOSE_ADDR         "235.6.7.8"           // �鲥�����ش�ת����ַ...
														  
#define DEF_SSL_PROTO               "https"               // Ĭ��Э��ͷ
#define DEF_WEB_PROTO               "http"                // Ĭ��Э��ͷ
#define DEF_SSL_PORT                443                   // Ĭ��https�˿�
#define DEF_WEB_PORT                80                    // Ĭ��http �˿�
														  
#define DEF_AUDIO_OUT_CHANNEL_NUM   1                     // Ĭ����Ƶ���š�ѹ��������
#define DEF_AUDIO_OUT_SAMPLE_RATE   16000                 // Ĭ����Ƶ���š�ѹ��������
#define DEF_AUDIO_OUT_BITRATE_AAC   32000                 // Ĭ�ϻ���������AACѹ���������
#define DEF_WEBRTC_AEC_NN           160                   // ������������������(��������) => ��������16000ʱ���պ���10����...
#define DEF_CAMERA_START_ID			1                     // Ĭ������ͷ��ʼID
#define DEF_MAX_CAMERA              8                     // Ĭ���������ͷ��Ŀ
#define LINGER_TIME					500	                  // SOCKETֹͣʱ������·��BUFF��յ�����ӳ�ʱ��

#define WM_WEB_LOAD_RESOURCE		(WM_USER + 108)
#define	WM_WEB_AUTH_RESULT			(WM_USER + 110)

typedef	map<string, string>			GM_MapData;
typedef map<int, GM_MapData>		GM_MapNodeCamera;     // int  => ��ָ���ݿ�DBCameraID

typedef struct
{
	string		strData;			// ֡����
	int			typeFlvTag;			// FLV_TAG_TYPE_AUDIO or FLV_TAG_TYPE_VIDEO
	bool		is_keyframe;		// �Ƿ��ǹؼ�֡
	uint32_t	dwSendTime;			// ����ʱ��(����)
	uint32_t	dwRenderOffset;		// ʱ���ƫ��ֵ
}FMS_FRAME;

enum ROLE_TYPE
{
	kRoleWanRecv   = 0,      // ���������߽�ɫ
	kRoleMultiRecv = 1,      // �鲥�����߽�ɫ
	kRoleMultiSend = 2,      // �鲥�����߽�ɫ
};

enum CAMERA_STATE
{
	kCameraOffLine = 0,		// δ����
	kCameraConnect = 1,		// ��������
	kCameraOnLine  = 2,		// ������
	kCameraPusher  = 3,     // ������
};

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
