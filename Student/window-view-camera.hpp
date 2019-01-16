
#pragma once

#include "HYDefine.h"
#include "OSThread.h"
#include <QMouseEvent>
#include "qt-display.hpp"
#include <util/threading.h>

class CViewLeft;
class CVideoPlay;
class CAudioPlay;
class CWebrtcAEC;
class CViewRender;
class CDataThread;
class CUDPSendThread;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
protected slots:
	void		onTriggerReadyToRecvFrame();
public:
	bool		IsCameraOffLine() { return ((m_nCameraState == kCameraOffLine) ? true : false); }
	bool		IsCameraOnLine() { return ((m_nCameraState == kCameraOnLine) ? true : false); }
	bool		IsCameraPreviewShow() { return m_bIsPreviewShow; }
	bool		IsCameraPreviewMute() { return m_bIsPreviewMute; }
	int			GetDBCameraID() { return m_nDBCameraID; }
public:
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
	bool		doCameraStart();
	bool		doCameraStop();
private:
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
	CViewLeft       *   m_lpViewLeft;       // 左侧父窗口对象...
	CWebrtcAEC      *   m_lpWebrtcAEC;      // 回音处理对象...
	CDataThread     *   m_lpDataThread;     // 数据基础类线程...
	CUDPSendThread  *   m_lpUDPSendThread;  // UDP推流线程...
	pthread_mutex_t     m_MutexPlay;        // 音视频回放互斥体...
	pthread_mutex_t     m_MutexAEC;         // 回音消除线程互斥体

	bool				m_bFindFirstVKey;	// 是否找到第一个视频关键帧标志...
	int64_t				m_sys_zero_ns;		// 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_start_pts_ms;		// 第一帧的PTS时间戳计时起点 => 毫秒...
	CVideoPlay      *   m_lpVideoPlay;      // 视频回放对象接口...
	CAudioPlay      *   m_lpAudioPlay;      // 音频回放对象接口...
	CViewRender     *   m_lpViewPlayer;     // 视频回放窗口对象...
};
