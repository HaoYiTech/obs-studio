
#pragma once

#include "tinyxml.h"
#include "HYDefine.h"
#include "OSThread.h"
#include <QMouseEvent>
#include "qt-display.hpp"
#include <util/threading.h>
#include <QNetworkAccessManager>

class QNetworkReply;
class CViewCamera;
class CCmdItem
{
public:
	CCmdItem(CViewCamera * lpViewCamera, CMD_ISAPI inCmdISAPI, int inSpeedVal = 0);
	~CCmdItem();
public:
	void            doUpdateCmdRequest();
	void            SetContentVal(string & inContent) { m_strContentVal = QString("%1").arg(inContent.c_str()); }
	void            SetNetReply(QNetworkReply * lpNetReply) { m_lpNetReply = lpNetReply; }
	QNetworkReply * GetNetReply() { return m_lpNetReply; }
	CMD_ISAPI       GetCmdISAPI() { return m_nCmdISAPI; }
	QString      &  GetRequestURL() { return m_strRequestURL; }
	QString      &  GetContentVal() { return m_strContentVal; }
	int             GetSpeedVal() { return m_nSpeedVal; }
private:
	QNetworkReply * m_lpNetReply;           // 命令相关的网络对象
	CViewCamera   * m_lpViewCamera;         // 命令对应的摄像头
	int             m_nSpeedVal;            // 命令对应的转速
	CMD_ISAPI		m_nCmdISAPI;			// 对应的命令编号
	QString         m_strRequestURL;		// 完整的请求命令连接地址
	QString         m_strContentVal;		// 需要发送的附加数据内容
};

typedef	deque<CCmdItem *>	GM_DeqCmd;		// 命令队列...

class CViewLeft;
class CVideoPlay;
class CAudioPlay;
class CWebrtcAEC;
class CViewRender;
class CDataThread;
class TiXmlElement;
class CUDPSendThread;
class QNetworkReply;
class QAuthenticator;
class QNetworkAccessManager;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
protected slots:
	void		onTriggerReadyToRecvFrame();
	void        onServerRttTrend(float inFloatDelta);
	void		onReplyFinished(QNetworkReply *reply);
	void        onAuthRequest(QNetworkReply *reply, QAuthenticator *authenticator);
public:
	bool		IsCameraOffLine() { return ((m_nCameraState <= kCameraConnect) ? true : false); }
	bool		IsCameraOnLine() { return ((m_nCameraState >= kCameraOnLine) ? true : false); }
	bool        IsCameraPusher() { return ((m_nCameraState >= kCameraPusher) ? true : false); }
	bool		IsCameraPreviewShow() { return m_bIsPreviewShow; }
	bool		IsCameraPreviewMute() { return m_bIsPreviewMute; }
	bool        IsCameraLoginISAPI() { return m_bIsLoginISAPI; }
	string   &  GetImageFilpVal() { return m_ImageFilpEnableVal; }
	int			GetDBCameraID() { return m_nDBCameraID; }
	int         GetChannelID();
public:
	bool        doPTZCmd(CMD_ISAPI inCMD, int inSpeedVal);
	void        doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf);
	void        doPushAudioAEC(FMS_FRAME & inFrame);
	void		doPushFrame(FMS_FRAME & inFrame);
	bool        doVolumeEvent(int inKeyItem);
	void		doUdpSendThreadStart();
	void		doUdpSendThreadStop();
	void        onUdpRecvThreadStop();
	void		doReadyToRecvFrame();
	void		doTogglePreviewShow();
	void		doTogglePreviewMute();
	void        doCameraLoginISAPI();
	bool		doCameraStart();
	bool		doCameraStop();
private:
	void        doParsePTZRange(TiXmlElement * lpXmlElem, POINT & outRange);
	void        doParseResult(CCmdItem * lpCmdItem, string & inXmlData);
	bool        doFirstCmdItem();
private:
	void        ClearAllCmd();
	void		DrawTitleArea();
	void		DrawRenderArea();
	void		DrawStatusText();
private:
	QString		GetRecvPullRate();
	QString		GetSendPushRate();
	QString		GetStreamPushUrl();
	bool		IsDataFinished();
	bool		IsFrameTimeout();
	void		CalcFlowKbps();
	void        BuildSendThread();
	void        ReBuildWebrtcAEC();
	void        doCreatePlayer();
	void        doDeletePlayer();
	void        doPushPlayer(FMS_FRAME & inFrame);
protected:
	void		paintEvent(QPaintEvent *event) override;
	void		timerEvent(QTimerEvent * inEvent) override;
	void		mousePressEvent(QMouseEvent *event) override;
private:
	CAMERA_STATE        m_nCameraState;		// 摄像头状态...
	uint32_t			m_dwTimeOutMS;		// 超时计时点...
	uint32_t			m_nCurRecvByte;		// 当前已接收字节数
	int					m_nRecvKbps;		// 拉流接收码率...
	int                 m_nFlowTimer;       // 码率检测时钟...
	int                 m_nDBCameraID;      // 通道在数据库中的编号...
	bool                m_bIsPreviewMute;   // 通道是否处于静音状态...
	bool                m_bIsPreviewShow;   // 通道是否正在预览画面...
	bool                m_bIsLoginISAPI;    // 通道是否成功登录IPC的ISAPI...
	CViewLeft       *   m_lpViewLeft;       // 左侧父窗口对象...
	CWebrtcAEC      *   m_lpWebrtcAEC;      // 回音处理对象...
	CDataThread     *   m_lpDataThread;     // 数据基础类线程...
	CUDPSendThread  *   m_lpUDPSendThread;  // UDP推流线程...
	pthread_mutex_t     m_MutexPlay;        // 音视频回放互斥体...
	pthread_mutex_t     m_MutexAEC;         // 回音消除线程互斥体
	pthread_mutex_t     m_MutexSend;        // 数据发送线程互斥体

	bool				m_bFindFirstVKey;	// 是否找到第一个视频关键帧标志...
	int64_t				m_sys_zero_ns;		// 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_start_pts_ms;		// 第一帧的PTS时间戳计时起点 => 毫秒...
	CVideoPlay      *   m_lpVideoPlay;      // 视频回放对象接口...
	CAudioPlay      *   m_lpAudioPlay;      // 音频回放对象接口...
	CViewRender     *   m_lpViewPlayer;     // 视频回放窗口对象...

	POINT				m_XRange = {0,0};	// ISAPI PTZ XRange
	POINT				m_YRange = {0,0};	// ISAPI PTZ YRange
	POINT				m_ZRange = {0,0};	// ISAPI PTZ ZRange
	POINT				m_FRange = {0,0};	// ISAPI PTZ Focus
	POINT				m_IRange = {0,0};	// ISAPI PTZ Iris
	string              m_ImageFilpStyle;
	string              m_ImageFilpEnableVal;
	string              m_ImageFilpEnableOpt;
	string              m_strVideoQualityType;    // CBR or VBR...
	int                 m_nMaxVideoKbps = 1024;   // 当前摄像头配置的最高码率...
	int                 m_nCBRVideoKbps = 1024;   // 当前摄像头配置的固定码率...
	int                 m_nVBRVideoKbps = 1024;   // 当前摄像头配置的动态码率...
	TiXmlDocument       m_XmlChannelDoc;          // 用来动态改变通道配置信息...

	bool                   m_bIsNetRunning = false;		// 是否正在执行命令
	GM_DeqCmd              m_deqCmd;					// 名列队列对象...
	QNetworkAccessManager  m_objNetManager;				// QT 网络管理对象...

	friend class CCmdItem;
};
