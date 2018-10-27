
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
class CViewPlayer;
class CDataThread;
class CUDPSendThread;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
signals:
	void		doTriggerReadyToRecvFrame();
protected slots:
	void		onTriggerReadyToRecvFrame();
public:
	bool		IsCameraOffLine() { return ((m_nCameraState == kCameraOffLine) ? true : false); }
	bool		IsCameraOnLine() { return ((m_nCameraState == kCameraOnLine) ? true : false); }
	bool		IsCameraPreview() { return m_bIsPreview; }
	int			GetDBCameraID() { return m_nDBCameraID; }
public:
	void        doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf);
	void        doPushAudioAEC(FMS_FRAME & inFrame);
	void		doPushFrame(FMS_FRAME & inFrame);
	void		doUdpSendThreadStart();
	void		doUdpSendThreadStop();
	void        onUdpRecvThreadStop();
	void		doReadyToRecvFrame();
	void		doTogglePreview();
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
	CAMERA_STATE        m_nCameraState;		// ����ͷ״̬...
	uint32_t			m_dwTimeOutMS;		// ��ʱ��ʱ��...
	uint32_t			m_nCurRecvByte;		// ��ǰ�ѽ����ֽ���
	int					m_nRecvKbps;		// ������������...
	int                 m_nFlowTimer;       // ���ʼ��ʱ��...
	int                 m_nDBCameraID;      // ͨ�������ݿ��еı��...
	bool                m_bIsPreview;       // ͨ���Ƿ�����Ԥ������...
	CViewLeft       *   m_lpViewLeft;       // ��ุ���ڶ���...
	CWebrtcAEC      *   m_lpWebrtcAEC;      // �����������...
	CDataThread     *   m_lpDataThread;     // ���ݻ������߳�...
	CUDPSendThread  *   m_lpUDPSendThread;  // UDP�����߳�...
	pthread_mutex_t     m_MutexPlay;        // ����Ƶ�طŻ�����...
	pthread_mutex_t     m_MutexAEC;         // ���������̻߳�����

	bool				m_bFindFirstVKey;	// �Ƿ��ҵ���һ����Ƶ�ؼ�֡��־...
	int64_t				m_sys_zero_ns;		// ϵͳ��ʱ��� => ����ʱ��� => ����...
	int64_t				m_start_pts_ms;		// ��һ֡��PTSʱ�����ʱ��� => ����...
	CVideoPlay      *   m_lpVideoPlay;      // ��Ƶ�طŶ���ӿ�...
	CAudioPlay      *   m_lpAudioPlay;      // ��Ƶ�طŶ���ӿ�...
	CViewPlayer     *   m_lpViewPlayer;     // ��Ƶ�طŴ��ڶ���...
};
