
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
class TiXmlElement;
class CUDPSendThread;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
protected slots:
	void		onTriggerReadyToRecvFrame();
public:
	bool		IsCameraOffLine() { return ((m_nCameraState <= kCameraConnect) ? true : false); }
	bool		IsCameraOnLine() { return ((m_nCameraState >= kCameraOnLine) ? true : false); }
	bool		IsCameraPreviewShow() { return m_bIsPreviewShow; }
	bool		IsCameraPreviewMute() { return m_bIsPreviewMute; }
	bool        IsCameraLoginISAPI() { return m_bIsLoginISAPI; }
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
	bool        doCameraLoginISAPI();
	bool		doCameraStart();
	bool		doCameraStop();
public:
	void        doCurlHeaderWrite(char * pData, size_t nSize);
	void        doCurlContent(char * pData, size_t nSize);
private:
	int         doCurlCommISAPI(const char * lpAuthHeader = NULL);
	bool        doCurlAuthISAPI();
	void        doParseWWWAuth(string & inHeader);
	void        doParsePTZRange(TiXmlElement * lpXmlElem, POINT & outRange);
	string      doCreateCNonce(int inLength = 16);
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
	bool                m_bIsPreviewMute;   // ͨ���Ƿ��ھ���״̬...
	bool                m_bIsPreviewShow;   // ͨ���Ƿ�����Ԥ������...
	bool                m_bIsLoginISAPI;    // ͨ���Ƿ�ɹ���¼IPC��ISAPI...
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
	CViewRender     *   m_lpViewPlayer;     // ��Ƶ�طŴ��ڶ���...

	GM_MapDict			m_MapHttpDict;		// HTTPЭ��ͷ�ֵ伯��...
	string				m_strUTF8Content;	// ͳһ��ISAPI��������...
	string              m_strAuthQop;		// ISAPI Digest qop
	string              m_strAuthRealm;		// ISAPI Digest realm
	string              m_strAuthNonce;		// ISAPI Digest nonce
	string              m_strCurlURI;		// ISAPI ��ǰ�����URI
	POINT				m_XRange = { -100, 100 };	// ISAPI PTZ XRange
	POINT				m_YRange = { -100, 100 };	// ISAPI PTZ YRange
	POINT				m_ZRange = { -100, 100 };	// ISAPI PTZ ZRange
};
