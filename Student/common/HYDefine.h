
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
#include "GMError.h"
#include "HYVersion.h"
#include <util/platform.h>
#include <util/circlebuf.h>

#define DEF_CAMERA_START_ID			1							// Ĭ������ͷ��ʼID
#define DEF_MAX_CAMERA              8							// Ĭ���������ͷ��Ŀ
#define DEF_MAIN_NAME				"�ɼ���"						// Ĭ�������ڱ��� => ��������Unicode�������Զ��ͻ��ΪUnicode��ʽ...
#define DEF_WEB_HOME				"https://www.myhaoyi.com"	// Ĭ��������վ => ������ https:// ����С����ӿ�...
#define DEF_WEB_PORT				80							// WebĬ�϶˿� 
#define DEF_CLOUD_CLASS				"http://edu.ihaoyi.cn"		// �ƽ��ҵ�ַ
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

#define MsgLogGM(nErr) blog(LOG_ERROR, "Error: %lu, %s(%d)", nErr, __FILE__, __LINE__)
