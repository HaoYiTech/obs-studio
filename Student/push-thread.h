
#pragma once

#include <QObject>
#include "OSMutex.h"
#include "HYDefine.h"

class CUDPSendThread;
class CRtspThread;
class CViewCamera;
class CPushThread : public QObject
{
	Q_OBJECT
public:
	CPushThread(CViewCamera * lpViewCamera);
	~CPushThread();
public:
	bool		InitThread();
	int			GetRecvPullKbps();
	int			GetSendPushKbps();
	QString		GetStreamPushUrl();
	void		PushFrame(FMS_FRAME & inFrame);
	void		onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
	bool        IsLiveState() { return ((m_lpUDPSendThread != NULL) ? true : false); }
private:
	void		CalcFlowKbps();
	bool		IsDataFinished();
	bool		IsFrameTimeout();
protected:
	void		timerEvent(QTimerEvent * inEvent) override;
private:
	int                 m_nDBCameraID;
	int					m_nRecvKbps;		// 拉流接收码率...
	int					m_nFlowTimer;		// 码率检测时钟...
	uint32_t			m_nCurRecvByte;		// 当前已接收字节数
	uint32_t			m_dwTimeOutMS;		// 超时计时点...
	OSMutex				m_Mutex;			// 数据互斥对象...
	CRtspThread    *    m_lpRtspThread;		// RTSP拉流对象...
	CViewCamera	   *	m_lpViewCamera;		// 摄像头窗口对象...
	CUDPSendThread *    m_lpUDPSendThread;  // UDP推流线程...
};
