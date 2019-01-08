
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

#define DEF_MULTI_DATA_ADDR         "234.5.6.7"                 // �鲥����Ƶ����ת����ַ...
#define DEF_MULTI_LOSE_ADDR         "235.6.7.8"                 // �鲥�����ش�ת����ַ...

#define DEF_WEB_CENTER				"https://www.myhaoyi.com"	// Ĭ��������վ(443) => ������ https:// ����С����ӿ�...
#define DEF_WEB_CLASS				"https://edu.ihaoyi.cn"		// Ĭ���ƽ�����վ(443) => ������ https:// ����С����ӿ�...
#define DEF_SSL_PROTO               "https"                     // Ĭ��Э��ͷ
#define DEF_WEB_PROTO               "http"                      // Ĭ��Э��ͷ
#define DEF_SSL_PORT                443                         // Ĭ��https�˿�
#define DEF_WEB_PORT                80                          // Ĭ��http �˿�

#define DEF_AUDIO_OUT_CHANNEL_NUM   1                           // Ĭ����Ƶ���š�ѹ��������
#define DEF_AUDIO_OUT_SAMPLE_RATE   16000                       // Ĭ����Ƶ���š�ѹ��������
#define DEF_AUDIO_OUT_BITRATE_AAC   32000                       // Ĭ�ϻ���������AACѹ���������
#define DEF_WEBRTC_AEC_NN           160                         // ������������������(��������) => ��������16000ʱ���պ���10����...
#define DEF_CAMERA_START_ID			1							// Ĭ������ͷ��ʼID
#define DEF_MAX_CAMERA              8							// Ĭ���������ͷ��Ŀ
#define LINGER_TIME					500							// SOCKETֹͣʱ������·��BUFF��յ�����ӳ�ʱ��

#define WM_WEB_LOAD_RESOURCE		(WM_USER + 108)
#define	WM_WEB_AUTH_RESULT			(WM_USER + 110)

typedef	map<string, string>			GM_MapData;
typedef map<int, GM_MapData>		GM_MapNodeCamera;			// int  => ��ָ���ݿ�DBCameraID

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

enum AUTH_STATE
{
	kAuthRegister  = 1,		// ��վע����Ȩ
	kAuthExpired   = 2,		// ��Ȩ������֤
};

enum STREAM_PROP
{
	kStreamDevice  = 0,		// ����ͷ�豸������ => camera
	kStreamMP4File = 1,		// MP4�ļ�������    => .mp4
	kStreamUrlLink = 2,		// URL����������    => rtsp/rtmp
};

enum CAMERA_STATE
{
	kCameraOffLine = 0,		// δ����
	kCameraConnect = 1,		// ��������
	kCameraOnLine  = 2,		// ������
	kCameraPusher  = 3,     // ������
};

// define Mini QRCode type...
enum {
	kQRCodeShop       = 1,  // �ŵ�С������
	kQRCodeStudent    = 2,  // ѧ����С������
	kQRCodeTeacher    = 3,  // ��ʦ��С������
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
