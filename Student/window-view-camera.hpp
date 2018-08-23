
#pragma once

#include "HYDefine.h"
#include <QMouseEvent>
#include "qt-display.hpp"

class CViewLeft;
class CSpeexAEC;
class CDataThread;
class CUDPSendThread;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
public:
	bool		IsCameraOffLine() { return ((m_nCameraState == kCameraOffLine) ? true : false); }
	bool		IsCameraOnLine() { return ((m_nCameraState == kCameraOnLine) ? true : false); }
	bool		IsCameraPreview() { return m_bIsPreview; }
	int			GetDBCameraID() { return m_nDBCameraID; }
public:
	void		onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
	void        doEchoCancel(void * lpBufData, int nBufSize);
	void		doPushFrame(FMS_FRAME & inFrame);
	void		doStartPushThread();
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
	QRect               m_rcRenderRect;     // ���ڻ�����Ⱦ�ľ�������...
	CViewLeft       *   m_lpViewLeft;       // ��ุ���ڶ���...
	CSpeexAEC       *   m_lpSpeexAEC;       // �����������...
	CDataThread     *   m_lpDataThread;     // ���ݻ������߳�...
	CUDPSendThread  *   m_lpUDPSendThread;  // UDP�����߳�...
};
