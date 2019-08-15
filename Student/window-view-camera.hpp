
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
	QNetworkReply * m_lpNetReply;           // ������ص��������
	CViewCamera   * m_lpViewCamera;         // �����Ӧ������ͷ
	int             m_nSpeedVal;            // �����Ӧ��ת��
	CMD_ISAPI		m_nCmdISAPI;			// ��Ӧ��������
	QString         m_strRequestURL;		// �����������������ӵ�ַ
	QString         m_strContentVal;		// ��Ҫ���͵ĸ�����������
};

typedef	deque<CCmdItem *>	GM_DeqCmd;		// �������...

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
	pthread_mutex_t     m_MutexSend;        // ���ݷ����̻߳�����

	bool				m_bFindFirstVKey;	// �Ƿ��ҵ���һ����Ƶ�ؼ�֡��־...
	int64_t				m_sys_zero_ns;		// ϵͳ��ʱ��� => ����ʱ��� => ����...
	int64_t				m_start_pts_ms;		// ��һ֡��PTSʱ�����ʱ��� => ����...
	CVideoPlay      *   m_lpVideoPlay;      // ��Ƶ�طŶ���ӿ�...
	CAudioPlay      *   m_lpAudioPlay;      // ��Ƶ�طŶ���ӿ�...
	CViewRender     *   m_lpViewPlayer;     // ��Ƶ�طŴ��ڶ���...

	POINT				m_XRange = {0,0};	// ISAPI PTZ XRange
	POINT				m_YRange = {0,0};	// ISAPI PTZ YRange
	POINT				m_ZRange = {0,0};	// ISAPI PTZ ZRange
	POINT				m_FRange = {0,0};	// ISAPI PTZ Focus
	POINT				m_IRange = {0,0};	// ISAPI PTZ Iris
	string              m_ImageFilpStyle;
	string              m_ImageFilpEnableVal;
	string              m_ImageFilpEnableOpt;
	string              m_strVideoQualityType;    // CBR or VBR...
	int                 m_nMaxVideoKbps = 1024;   // ��ǰ����ͷ���õ��������...
	int                 m_nCBRVideoKbps = 1024;   // ��ǰ����ͷ���õĹ̶�����...
	int                 m_nVBRVideoKbps = 1024;   // ��ǰ����ͷ���õĶ�̬����...
	TiXmlDocument       m_XmlChannelDoc;          // ������̬�ı�ͨ��������Ϣ...

	bool                   m_bIsNetRunning = false;		// �Ƿ�����ִ������
	GM_DeqCmd              m_deqCmd;					// ���ж��ж���...
	QNetworkAccessManager  m_objNetManager;				// QT ����������...

	friend class CCmdItem;
};
