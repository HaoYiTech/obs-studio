
#pragma once

#include <QObject>
#include "OSMutex.h"
#include "HYDefine.h"

class CSpeexAEC;
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
	void        onRtspReady();
	int			GetRecvPullKbps();
	int			GetSendPushKbps();
	QString		GetStreamPushUrl();
	void		PushFrame(FMS_FRAME & inFrame);
	void		onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
	bool        IsLiveState() { return ((m_lpUDPSendThread != NULL) ? true : false); }
public:
	string	 	GetAVCHeader();
	string	 	GetAACHeader();
	string   	GetVideoSPS();
	string   	GetVideoPPS();
	int			GetVideoFPS();
	int			GetVideoWidth();
	int			GetVideoHeight();
	int			GetAudioRateIndex();
	int			GetAudioChannelNum();
	int			GetDBCameraID() { return m_nDBCameraID; }
private:
	void		CalcFlowKbps();
	bool		IsDataFinished();
	bool		IsFrameTimeout();
protected:
	void		timerEvent(QTimerEvent * inEvent) override;
private:
	int                 m_nDBCameraID;
	int					m_nRecvKbps;		// ������������...
	int					m_nFlowTimer;		// ���ʼ��ʱ��...
	uint32_t			m_nCurRecvByte;		// ��ǰ�ѽ����ֽ���
	uint32_t			m_dwTimeOutMS;		// ��ʱ��ʱ��...
	//OSMutex			m_Mutex;			// ���ݻ������...
	CRtspThread    *    m_lpRtspThread;		// RTSP��������...
	CViewCamera	   *	m_lpViewCamera;		// ����ͷ���ڶ���...
	CUDPSendThread *    m_lpUDPSendThread;  // UDP�����߳�...
	CSpeexAEC      *    m_lpSpeexAEC;       // ������������...
};