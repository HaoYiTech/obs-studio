
#include <QPainter>
#include "webrtc-aec.h"
#include "student-app.h"
#include "play-thread.h"
#include "pull-thread.h"
#include "web-thread.h"
#include "FastSession.h"
#include "UDPSendThread.h"
#include "window-view-left.hpp"
#include "window-view-render.hpp"
#include "window-view-camera.hpp"
#include "HTTPProtocol.h"
#include "StringParser.h"
#include "tinyxml.h"
#include <curl.h>
#include <md5.h>

#define LINE_SPACE_PIXEL		6
#define LEFT_SPACE_PIXEL		20
#define RIGHT_SPACE_PIXEL		20
#define NOTICE_FONT_HEIGHT		12
#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_LIVE_COLOR        QColor(255,140,0)
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)
#define DEF_WWW_AUTH			"Digest"

CViewCamera::CViewCamera(QWidget *parent, int nDBCameraID)
  : OBSQTDisplay(parent, 0)
  , m_nCameraState(kCameraOffLine)
  , m_nDBCameraID(nDBCameraID)
  , m_bIsLoginISAPI(false)
  , m_bIsPreviewMute(true)
  , m_bIsPreviewShow(false)
  , m_bFindFirstVKey(false)
  , m_lpUDPSendThread(NULL)
  , m_lpDataThread(NULL)
  , m_lpViewPlayer(NULL)
  , m_lpViewLeft(NULL)
  , m_lpWebrtcAEC(NULL)
  , m_lpVideoPlay(NULL)
  , m_lpAudioPlay(NULL)
  , m_start_pts_ms(-1)
  , m_sys_zero_ns(-1)
  , m_nCurRecvByte(0)
  , m_dwTimeOutMS(0)
  , m_nFlowTimer(-1)
  , m_nRecvKbps(0)
{
	// ���游���ڶ���...
	ASSERT(m_nDBCameraID > 0 && parent != NULL);
	m_lpViewLeft = qobject_cast<CViewLeft*>(parent);
	ASSERT(parent != NULL && m_lpViewLeft != NULL);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	// ���ô��ڱ����������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
	// ע�⣺ֻ��QT�̵߳��в�������ʱ�Ӷ���...
	// ����һ����ʱ���ʱ�� => ÿ��1��ִ��һ��...
	m_nFlowTimer = this->startTimer(1 * 1000);
	// ��ʼ�����������������Ͳ����̻߳������...
	pthread_mutex_init_value(&m_MutexPlay);
	pthread_mutex_init_value(&m_MutexAEC);
}

CViewCamera::~CViewCamera()
{
	// ���ʱ����Ч��ɾ��ʱ�Ӷ���...
	if (m_nFlowTimer >= 0) {
		killTimer(m_nFlowTimer);
		m_nFlowTimer = -1;
	}
	// ɾ��UDP�����߳� => ����ʹ����...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// ɾ����Ƶ������������ => ����ʹ����...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
	// ɾ��������ȡ�̶߳���...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// ����Լ������Ǹ����㴰�ڣ���Ҫ����...
	App()->doResetFocus(this);
	// �ͷ�����Ƶ�����̶߳���...
	this->doDeletePlayer();
	// �ͷŻ��������������Ͳ��Ż������...
	pthread_mutex_destroy(&m_MutexPlay);
	pthread_mutex_destroy(&m_MutexAEC);
}

void CViewCamera::timerEvent(QTimerEvent * inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nFlowTimer) {
		this->CalcFlowKbps();
	}
}

void CViewCamera::CalcFlowKbps()
{
	// ����ʵ�ʵĽ������� => Kbps...
	m_nRecvKbps = (m_nCurRecvByte * 8) / 1024;
	m_nCurRecvByte = 0;
	// ������ڽ��б��ػطţ���Ҫ�ֶ����£�������ʾ...
	if (m_lpViewPlayer != NULL) {
		this->GetRecvPullRate();
		this->GetSendPushRate();
		return;
	}
	// ͨ����������(�������ӻ�������)������Ҫ���½���...
	if (m_nCameraState != kCameraOffLine) {
		this->update();
	}
}

// ע�⣺����ʱ�ӷ��� CPushThread::timerEvent() => ÿ�����һ��...
void CViewCamera::paintEvent(QPaintEvent *event)
{
	// ���Ʊ���������...
	this->DrawTitleArea();
	// ����������֮��Ĵ�������...
	this->DrawRenderArea();
	// ��ʾͨ��������״̬��Ϣ...
	this->DrawStatusText();
	//QWidget::paintEvent(event);
}

void CViewCamera::mousePressEvent(QMouseEvent *event)
{
	// ���������Ҽ��¼���� => �������¼�...
	Qt::MouseButton theBtn = event->button();
	if (theBtn == Qt::LeftButton || theBtn == Qt::RightButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewCamera::DrawTitleArea()
{
	// �滭����״̬�²Ž��л���...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ��ȡ�������ڿͻ������δ�С...
	QRect & rcRect = this->rect();
	// ������������������ɫ...
	painter.fillRect(1, 1, rcRect.width()-2, TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// ���ñ�����������ɫ...
	painter.setPen(TITLE_TEXT_COLOR);
	// ������������� => �ر�ע�⣺���������½�����...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT) / 2 + 1;
	// �Ȼ�ȡ����ͷ�ı������ƣ��ٽ����ַ�������...
	QString strTitleContent = App()->GetCameraQName(m_nDBCameraID);
	QString strTitleText = QString("%1%2 - %3").arg(QStringLiteral("ID��")).arg(m_nDBCameraID).arg(strTitleContent);
	painter.drawText(nPosX, nPosY, strTitleText);
	// ��������LIVE������Ϣ => UDP�߳���Ч...
	if (m_lpUDPSendThread != NULL) {
		painter.setPen(TITLE_LIVE_COLOR);
		QFontMetrics & fontMetrics = painter.fontMetrics();
		QString strLive = QTStr("Camera.Window.Live");
		nPosX = rcRect.width() - 2 * fontMetrics.width(strLive);
		painter.drawText(nPosX, nPosY, strLive);
	}
}

void CViewCamera::DrawRenderArea()
{
	// �滭����״̬�²Ž��л���...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	QRect rcRect = this->rect();
	// ���������α߿�...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// ע�⣺������ƾ��ε���ʼ�����-1��ʼ��QSplitter�Ķ���������(1,1)...
	// ���Ʊ߿����û�����ɫ�����ž�������...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// �����Ⱦ�����ο�...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	painter.fillRect(rcRect, m_bkColor);
	// ע�⣺����ʹ��PS��ͼ��Ŵ�ȷ�ϴ�С...
	// ��Ⱦ������Ч��������Ⱦ���ڵľ���λ��...
	if (m_lpViewPlayer != NULL) {
		m_lpViewPlayer->doResizeRect(rcRect);
	}
}

void CViewCamera::DrawStatusText()
{
	// �����Ƶ�طŴ�����Ч����Ҫ��������...
	if (m_lpViewPlayer != NULL)
		return;
	ASSERT(m_lpViewPlayer == NULL);
	// �滭����״̬�²Ž��л���...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ������������ͻ�����ɫ...
	QRect rcRect = this->rect();
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	painter.setPen(QPen(STATUS_TEXT_COLOR));
	QFontMetrics & fontMetrics = painter.fontMetrics();
	// ���㶥�����...
	int nLineSpace = LINE_SPACE_PIXEL;
	int nTopSpace = (rcRect.height() - TITLE_FONT_HEIGHT * 5 - nLineSpace * 4) / 2 - 20;
	// ��ʾͨ��״̬������ => �ر�ע�⣺���������½�����...
	int xNamePos = rcRect.left() + LEFT_SPACE_PIXEL;
	int yNamePos = rcRect.top() + nTopSpace + TITLE_FONT_HEIGHT;
	QString strTitleName = QTStr("Camera.Window.StatName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾͨ��״̬��������...
	int xDataPos = xNamePos + fontMetrics.width(strTitleName);
	int yDataPos = yNamePos;
	// ��ȡͨ��״̬����...
	QString strDataValue;
	switch(m_nCameraState)
	{
	case kCameraOffLine: strDataValue = QTStr("Camera.Window.OffLine"); break;
	case kCameraConnect: strDataValue = QTStr("Camera.Window.Connect"); break;
	case kCameraOnLine:  strDataValue = QTStr("Camera.Window.OnLine");  break;
	case kCameraPusher:  strDataValue = QTStr("Camera.Window.OnLine");  break;
	default:             strDataValue = QTStr("Camera.Window.OffLine"); break;
	}
	painter.drawText(xDataPos, yDataPos, strDataValue);
	// ��ʾ������ַ������ => ע�������½�����...
	strTitleName = QTStr("Camera.Window.PullName");
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// �������ֶ������Զ�����...
	QRect rcText;
	QTextOption txtOption(Qt::AlignLeft);
	txtOption.setWrapMode(QTextOption::WrapAnywhere);
	// ����������ַ��ʾ����λ�� => ע�������½�����...
	rcText.setTop(yNamePos - TITLE_FONT_HEIGHT - 2);
	rcText.setLeft(xNamePos + fontMetrics.width(strTitleName));
	rcText.setRight(rcRect.right() - RIGHT_SPACE_PIXEL);
	rcText.setBottom(rcText.top() + TITLE_FONT_HEIGHT * 4);
	// ��ȡ������ַ�ľ�������...
	strDataValue = App()->GetCameraPullUrl(m_nDBCameraID);
	// ��ʾ������ַ��Ϣ�����ۼ���һ����ʾλ��...
	painter.drawText(rcText, strDataValue, txtOption);
	yNamePos += TITLE_FONT_HEIGHT * 4 + nLineSpace;
	// ��ʾ�������ʵı�����...
	strTitleName = QTStr("Camera.Window.PullRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ�������ʵľ�������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetRecvPullRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// ��ʾ������ַ������...
	strTitleName = QTStr("Camera.Window.PushName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ������ַ��������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetStreamPushUrl();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// ��ʾ�������ʱ�����...
	strTitleName = QTStr("Camera.Window.PushRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ�������ʾ�������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetSendPushRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
}

QString CViewCamera::GetStreamPushUrl()
{
	if (m_lpUDPSendThread == NULL) {
		return QTStr("Camera.Window.None");
	}
	string & strServerAddr = m_lpUDPSendThread->GetServerAddrStr();
	QString strQServerStr = QString::fromUtf8(strServerAddr.c_str());
	int nServerPort = m_lpUDPSendThread->GetServerPortInt();
	return QString("udp://%1/%2").arg(strQServerStr).arg(nServerPort);
}

bool CViewCamera::IsDataFinished()
{
	return ((m_lpDataThread != NULL) ? m_lpDataThread->IsFinished() : true);
}

bool CViewCamera::IsFrameTimeout()
{
	// �����ж������߳��Ƿ��Ѿ�������...
	if (this->IsDataFinished())
		return true;
	// һֱû�������ݵ������ 1 ���ӣ����ж�Ϊ��ʱ...
	int nWaitMinute = 1;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}

QString CViewCamera::GetRecvPullRate()
{
	// ����Ĭ�ϵ���������ֵ...
	int nRecvKbps = m_nRecvKbps;
	// ���״̬��������(�������ӻ�������)������Ҫ�ж��Ƿ�ʱ����ʱ����Ϊ -1 ...
	if (m_nCameraState != kCameraOffLine) {
		nRecvKbps = (this->IsFrameTimeout() ? -1 : nRecvKbps);
	}
	// �������Ϊ��������Ҫֹͣͨ����ͬʱ�����²˵�...
	if ( nRecvKbps < 0 ) {
		// ��ֹͣͨ��...
		this->doCameraStop();
		// ������㴰�ڻ��ǵ�ǰ���ڣ���Ҫ���¹������˵�״̬...
		if (m_lpViewLeft->GetFocusID() == m_nDBCameraID) {
			emit m_lpViewLeft->enableCameraStart(true);
			emit m_lpViewLeft->enableCameraStop(false);
		}
		// ���ý�������...
		nRecvKbps = 0;
	}
	// ���ؽ������ʵ��ַ�������...
	return QString("%1 Kbps").arg(nRecvKbps);
}

QString CViewCamera::GetSendPushRate()
{
	// ��ȡ�������ʵľ������� => ֱ�Ӵ�UDP�����̵߳��л�ȡ => ע�⣺Ĭ��ֵ��0...
	int nSendKbps = ((m_lpUDPSendThread != NULL) ? m_lpUDPSendThread->GetSendTotalKbps() : 0);
	// ���Ϊ��������Ҫɾ��UDP�����̶߳���...
	if (nSendKbps < 0) {
		// ��ɾ��UDP�����̶߳���...
		this->doUdpSendThreadStop();
		// ������������...
		nSendKbps = 0;
	}
	// ���ؽ������ʵ��ַ�������...
	return QString("%1 Kbps").arg(nSendKbps);
}

// �����Ҳಥ���߳��Ѿ�ֹ֪ͣͨ...
void CViewCamera::onUdpRecvThreadStop()
{
	// ֪ͨ����������������ؽ�����...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		m_lpWebrtcAEC->onUdpRecvThreadStop();
	}
	pthread_mutex_unlock(&m_MutexAEC);
	/*// ֪ͨ��Ƶ���Ŷ�����Ҫ�����ؽ� => �Ҳ�SDL���˳���������SDL��Ӱ��...
	pthread_mutex_lock(&m_MutexPlay);
	if (m_lpVideoPlay != NULL) {
		m_lpVideoPlay->onUdpRecvThreadStop();
	}
	pthread_mutex_unlock(&m_MutexPlay);*/
}

void CViewCamera::doUdpSendThreadStart()
{
	// ���ͨ������״̬��������״̬��ֱ�ӷ���...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CameraState: %d", m_nCameraState);
		return;
	}
	// �������ͷ�����߳���Ч����ӡ���󣬷���...
	if (m_lpDataThread == NULL) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CDataThread is NULL");
		return;
	}
	// ��������߳��Ѿ������ˣ�ֱ�ӷ���...
	if (m_lpUDPSendThread != NULL)
		return;
	ASSERT(m_lpUDPSendThread == NULL);
	// �ؽ������������� => ͨ����������...
	this->ReBuildWebrtcAEC();
	// ����UDP�����̶߳��� => ͨ����������...
	this->BuildSendThread();
}

void CViewCamera::doUdpSendThreadStop()
{
	// ֻ�е������߳���Ч�����޸�ͨ��״̬Ϊ����״̬...
	if (m_lpUDPSendThread != NULL) {
		m_nCameraState = kCameraOnLine;
	}
	// �����ͨ��ֹͣ�������� => ɾ�������߳�...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// ��ɾ��������������...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
	// ���½��棬ǿ�ƻ��Ʊ�����...
	this->update();
}

void CViewCamera::BuildSendThread()
{
	// ͨ������������״̬...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "== BuildSendThread Error for CameraState ==");
		return;
	}
	ASSERT(m_nCameraState == kCameraOnLine);
	// ����webrtc��DA-AEC�����������������Ч...
	if (m_lpWebrtcAEC == NULL) {
		blog(LOG_INFO, "== BuildSendThread Error for WebrtcAEC ==");
		return;
	}
	// ��Ƶ������Ҫ���⴦��ͳһʹ��ȫ�ֵ���Ƶ�趨����Ϊ�����������������Ч...
	int nAudioRateIndex = App()->GetAudioRateIndex();
	int nAudioChannelNum = App()->GetAudioChannelNum();
	// ����UDP�����̣߳�ʹ�÷��������ݹ����Ĳ���...
	int nRoomID = atoi(App()->GetRoomIDStr().c_str());
	string & strUdpAddr = App()->GetUdpAddr();
	int nUdpPort = App()->GetUdpPort();
	int nDBCameraID = m_nDBCameraID;
	m_lpUDPSendThread = new CUDPSendThread(m_lpDataThread, nRoomID, nDBCameraID);
	// �����ʼ��UDP�����߳�ʧ�ܣ�ɾ���Ѿ������Ķ���...
	if (!m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort, nAudioRateIndex, nAudioChannelNum)) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
		return;
	}
	// ���õ�ǰͨ��Ϊ��������״̬...
	m_nCameraState = kCameraPusher;
	// ���½��棬ǿ�ƻ��Ʊ�����...
	this->update();
}

void CViewCamera::ReBuildWebrtcAEC()
{
	// ͨ������������״̬...
	if (m_nCameraState != kCameraOnLine)
		return;
	ASSERT(m_nCameraState == kCameraOnLine);
	// ���뻥�Ᵽ������...
	pthread_mutex_lock(&m_MutexAEC);
	// �ؽ�����ʼ����Ƶ������������...
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	// ��ȡ��Ƶ��صĸ�ʽͷ��Ϣ���Լ�������������...
	int nInRateIndex = m_lpDataThread->GetAudioRateIndex();
	int nInChannelNum = m_lpDataThread->GetAudioChannelNum();
	int nOutSampleRate = App()->GetAudioSampleRate();
	int nOutChannelNum = App()->GetAudioChannelNum();
	int nOutBitrateAAC = App()->GetAudioBitrateAAC();
	// ��������ʼ��������������...
	m_lpWebrtcAEC = new CWebrtcAEC(this);
	// ��ʼ��ʧ�ܣ�ɾ��������������...
	if (!m_lpWebrtcAEC->InitWebrtc(nInRateIndex, nInChannelNum, nOutSampleRate, nOutChannelNum, nOutBitrateAAC)) {
		delete m_lpWebrtcAEC; m_lpWebrtcAEC = NULL;
		blog(LOG_INFO, "== CWebrtcAEC::InitWebrtc() => Error ==");
	}
	// �˳����Ᵽ������...
	pthread_mutex_unlock(&m_MutexAEC);
}

// �ź�doTriggerReadyToRecvFrame��ִ�к���...
void CViewCamera::onTriggerReadyToRecvFrame()
{
	// ����ת�������㱨ͨ����Ϣ��״̬ => �ؽ��ɹ�֮���ٷ�������...
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	if (lpRemoteSession != NULL) {
		lpRemoteSession->doSendStartCameraCmd(m_nDBCameraID);
	}
	// ���ýӿ�֪ͨ������ => �޸�ͨ��״̬ => �ؽ��ɹ�֮���ٷ�������...
	CWebThread * lpWebThread = App()->GetWebThread();
	if (lpWebThread != NULL) {
		lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOnLine);
	}
	// ����ͨ��״̬Ϊ����������״̬...
	m_nCameraState = kCameraOnLine;
}

// ����������źŲۣ�������ͬһ�̣߳�����QTSocket�Ķ��̷߳��ʹ���...
void CViewCamera::doReadyToRecvFrame()
{
	QMetaObject::invokeMethod(this, "onTriggerReadyToRecvFrame");
}

void CViewCamera::doPushFrame(FMS_FRAME & inFrame)
{
	// ���ͨ��������״̬С������Ͷ������...
	if (m_nCameraState < kCameraOnLine)
		return;
	ASSERT(m_nCameraState >= kCameraOnLine);
	// ����ȡ��������Ƶ���ݽ��б��ؽ���ط�...
	this->doPushPlayer(inFrame);
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	// ���ͨ����������״̬�����ܽ������ݷ���...
	if (m_nCameraState != kCameraPusher)
		return;
	ASSERT(m_nCameraState == kCameraPusher);
	// ���UDP�����߳���Ч�����ң������߳�û�з�������ӵ����������������...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		// �������Ƶ���������������Ȼ��Ч��Ͷ�ݸ�������������...
		// ע�⣺����Ͷ�ݸ���������������Ϊ��Ƶ��ʽҪת��...
		if (inFrame.typeFlvTag == PT_TAG_AUDIO) {
			pthread_mutex_lock(&m_MutexAEC);
			if (m_lpWebrtcAEC != NULL) {
				m_lpWebrtcAEC->PushMicFrame(inFrame);
			}
			pthread_mutex_unlock(&m_MutexAEC);
		}
		// �������Ƶ��Ͷ�ݵ�UDP�����̣߳����д������...
		if (inFrame.typeFlvTag == PT_TAG_VIDEO) {
			m_lpUDPSendThread->PushFrame(inFrame);
		}
	}
}

// �ѻ�������֮���AAC��Ƶ�����ظ�UDP���Ͷ�����...
void CViewCamera::doPushAudioAEC(FMS_FRAME & inFrame)
{
	// ���UDP�����߳���Ч�����ң������߳�û�з�������ӵ����������������...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
}

// ��Ͷ�ݵ���������PCM��Ƶ�����������������...
void CViewCamera::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		m_lpWebrtcAEC->PushHornPCM(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf);
	}
	pthread_mutex_unlock(&m_MutexAEC);
}

size_t procCurlContent(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CViewCamera * lpViewCamera = (CViewCamera*)stream;
	lpViewCamera->doCurlContent((char*)ptr, size * nmemb);
	return size * nmemb;
}

size_t procCurlHeaderWrite(char *ptr, size_t size, size_t nmemb, void *stream)
{
	CViewCamera * lpViewCamera = (CViewCamera*)stream;
	lpViewCamera->doCurlHeaderWrite(ptr, size * nmemb);
	return size * nmemb;
}

// ע�⣺ÿ�ν���һ��HTTPЭ��ͷ...
void CViewCamera::doCurlHeaderWrite(char * pData, size_t nSize)
{
	StrPtrLen	 theHoleLine;
	StrPtrLen	 theCurPtr(pData, nSize);
	StringParser thePeek(&theCurPtr);
	StringParser parser(&theCurPtr);
	// ������������һ��,����,�������ݶ���...
	if (!thePeek.GetThruEOL(&theHoleLine))
		return;
	StrPtrLen	theKeyWord;
	Bool16		isStreamOK;
	// ����HTTPЭ��ͷ�ı�ʶ�ַ���...
	if (!parser.GetThru(&theKeyWord, ':'))
		return;
	// ��ʶ����������ո�����ȥ...
	if (parser.PeekFast() == ' ') {
		isStreamOK = parser.Expect(' ');
		ASSERT(isStreamOK);
	}
	// ����HTTPЭ��ͷ������ֵ...
	string		strValue;
	StrPtrLen	theHeaderVal;
	UInt32		theKeyIndex = 0;
	if (!parser.GetThruEOL(&theHeaderVal))
		return;
	strValue.assign(theHeaderVal.Ptr, theHeaderVal.Len);
	theKeyIndex = HTTPProtocol::GetHeader(theKeyWord);
	if (theKeyIndex == httpIllegalHeader)
		return;
	// �����������������Ȩ��Ϣ����Ҫ��������...
	if (theKeyIndex == httpWWWAuthenticateHeader) {
		this->doParseWWWAuth(strValue);
	}
	// ���HTTPЭ��ͷ�Ѿ����ڣ���Ҫ�ۼӣ��÷ֺ���Ϊ�ָ���...
	GM_MapDict::iterator itorItem = m_MapHttpDict.find(theKeyIndex);
	if (itorItem != m_MapHttpDict.end()) {
		string strDict = itorItem->second;
		strDict.append(_T("\r\n"));
		strDict.append(strValue);
		strValue = strDict;
	}
	// �����ռ�����HTTPЭ�����ݱ��浽�ֵ䵱��...
	m_MapHttpDict[theKeyIndex] = strValue;
}

void CViewCamera::doParseWWWAuth(string & inHeader)
{
	blog(LOG_INFO, "curl WWW-AUTH => %s", inHeader.c_str());
	StrPtrLen	 theCurPtr((char*)inHeader.c_str(), inHeader.size());
	StringParser parser(&theCurPtr);
	StrPtrLen	 theKeyWord;
	Bool16		 isStreamOK;
	// ��ȡ������Ȩģʽ���Կո����...
	if (!parser.GetThru(&theKeyWord, ' '))
		return;
	// �������Digestģʽ��ֱ�Ӷ���������...
	if (strnicmp(DEF_WWW_AUTH, theKeyWord.Ptr, theKeyWord.Len) != 0)
		return;
	// ������ֵ����Ϣ => key=value, or key,
	StrPtrLen	theItem, theValue;
	while (parser.GetDataRemaining() > 0) {
		parser.GetThru(&theItem, ',');
		if (theItem.Ptr == NULL || theItem.Len <= 0)
			continue;
		StringParser theSubParse(&theItem);
		while (theSubParse.GetDataRemaining() > 0) {
			// �ų�keyǰ��Ŀո������Ϣ...
			if (theSubParse.PeekFast() == ' ') {
				theSubParse.Expect(' ');
			}
			// ������key�ֶΣ��ԵȺ�Ϊ��������...
			theSubParse.GetThru(&theKeyWord, '=');
			if (theKeyWord.Ptr == NULL || theKeyWord.Len <= 0)
				continue;
			// ������value����������ţ���Ҫȥ������...
			if (theSubParse.PeekFast() == '"') {
				theSubParse.Expect('"');
				theSubParse.GetThru(&theValue, '"');
			} else {
				theSubParse.GetThru(&theValue, ',');
			}
			// ������������ݹ�������Ȩ�ض��ֶ���Ϣ...
			if (strnicmp("qop", theKeyWord.Ptr, theKeyWord.Len) == 0) {
				m_strAuthQop.assign(theValue.Ptr, theValue.Len);
			} else if (strnicmp("realm", theKeyWord.Ptr, theKeyWord.Len) == 0) {
				m_strAuthRealm.assign(theValue.Ptr, theValue.Len);
			} else if (strnicmp("nonce", theKeyWord.Ptr, theKeyWord.Len) == 0) {
				m_strAuthNonce.assign(theValue.Ptr, theValue.Len);
			}
		}
	}
}

void CViewCamera::doCurlContent(char * pData, size_t nSize)
{
	m_strUTF8Content.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE����г������ƣ�̫����ض�...
}

// ����16���ַ��Ŀͻ�������ַ���...
string CViewCamera::doCreateCNonce(int inLength/* = 16*/)
{
	string strCNonce;
	char * lpStrPtr = NULL;
	strCNonce.resize(inLength);
	lpStrPtr = (char*)strCNonce.c_str();
	srand((unsigned)time(NULL));
	static char CCH[] = "0123456789abcdef";
	for (int i = 0; i < inLength; ++i) {
		int x = rand() / (RAND_MAX / (sizeof(CCH) - 1));
		lpStrPtr[i] = CCH[x];
	}
	return strCNonce;
}

// ͨ�õ�ISAPI���ýӿں��� => ���� http ״̬��...
int CViewCamera::doCurlCommISAPI(const char * lpAuthHeader/* = NULL*/)
{
	ASSERT(m_strCurlURI.size() > 0);
	// �ȶ��ϴν����Ļ����������...
	m_strUTF8Content.clear();
	m_MapHttpDict.clear();
	// ��ȡ��ǰͨ�������ò���...
	GM_MapData theMapData;
	char strUrl[MAX_PATH] = { 0 };
	App()->GetCamera(m_nDBCameraID, theMapData);
	sprintf(strUrl, "http://%s%s", theMapData["device_ip"].c_str(), m_strCurlURI.c_str());
	// ��������˸�����ȨЭ��ͷ����Ҫ���⴦��...
	struct curl_slist *header = nullptr;
	if (lpAuthHeader != NULL) {
		header = curl_slist_append(header, lpAuthHeader);
	}
	// ����Curl�ӿڣ�ʹ��IPC��ISAPI�ӿ�...
	long     responseCode = 0;
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	char     error_in[CURL_ERROR_SIZE] = { 0 };
	KT_HTTPStatusIndex nStatusIndex = httpOK;
	do {
		if (curl == NULL) break;
		// �趨curl���������� GET ģʽ...
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		curl_easy_setopt(curl, CURLOPT_POST, false);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		if (header != NULL) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
		} else {
			curl_easy_setopt(curl, CURLOPT_HEADER, false);
		}
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_in);
		// ��ȡ���ص�httpЭ��ͷ��Ϣ...
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, procCurlHeaderWrite);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)this);
		// �������ռ���վ���ص�����...
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procCurlContent);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			blog(LOG_INFO, "curl error => %s", error_in);
		}
		// ��ȡhttp���صĴ�������...
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
		nStatusIndex = HTTPProtocol::GetStatusIndex(responseCode);
	} while (false);
	// �ͷŸ��ӵ�httpЭ��ͷ��Ϣ...
	if (header != NULL) {
		curl_slist_free_all(header);
	}
	// �ͷ���Դ...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	return nStatusIndex;
}

// ����ʹ��ISAPI��¼IPC����ͷ...
bool CViewCamera::doCameraLoginISAPI()
{
	// ���ͨ����������״̬������ʧ��...
	if (this->IsCameraOffLine())
		return false;
	// ͨ���Ѿ��ɹ���¼ISAPI�����سɹ�...
	if (this->IsCameraLoginISAPI())
		return true;
	ASSERT(!this->m_bIsLoginISAPI);
	ASSERT(this->IsCameraOnLine());
	// �趨URI������ͨ�ýӿڷ���...
	KT_HTTPStatusIndex nStatusIndex = httpOK;
	m_strCurlURI = "/ISAPI/PTZCtrl/channels/1/capabilities";
	nStatusIndex = this->doCurlCommISAPI();
	// ��ӡ״̬��Ϣ���Ա���Ը���...
	blog(LOG_INFO, "curl ISAPI Login => %d, %s", HTTPProtocol::GetStatusCode(nStatusIndex), HTTPProtocol::GetStatusCodeString(nStatusIndex).Ptr);
	// ���״̬��ΪOK���趨��¼�ɹ�״̬...
	if (nStatusIndex == httpOK) {
		m_bIsLoginISAPI = true;
		return true;
	}
	// �������δ��Ȩ״̬�룬ֱ�ӷ���...
	if (nStatusIndex != httpUnAuthorized)
		return false;
	// �����δ��Ȩ״̬�룬��Ҫ�����ض���Ȩ��Ϣ...
	ASSERT(nStatusIndex == httpUnAuthorized);
	return this->doCurlAuthISAPI();
}

bool CViewCamera::doCurlAuthISAPI()
{
	// ���û��realm��nonce������ʧ�ܣ�qop����û��...
	if (m_strAuthRealm.size() <= 0 || m_strAuthNonce.size() <= 0)
		return false;
	// ����һЩԤ���趨�ĳ���...
	string strMethod = "GET";
	string strNCount = "00000001";
	string strCNonce = this->doCreateCNonce();
	// ��ȡ��ŵ�ͨ������...
	GM_MapData theMapData;
	char szBuffer[MAX_PATH * 3] = { 0 };
	App()->GetCamera(m_nDBCameraID, theMapData);
	// ׼������response��Ҫ�ı�����Ϣ => md5��������...
	MD5	md5; string strHA1, strHA2, strResponse;
	// ��ʼ���� HA1=MD5(username:realm:password)
	sprintf(szBuffer, "%s:%s:%s", theMapData["device_user"].c_str(), m_strAuthRealm.c_str(), theMapData["device_pass"].c_str());
	md5.reset(); md5.update(szBuffer, strlen(szBuffer));
	strHA1 = md5.toString();
	// ��ʼ���� HA2=MD5(method:digestURI) => md5��������...
	sprintf(szBuffer, "%s:%s", strMethod.c_str(), m_strCurlURI.c_str());
	md5.reset(); md5.update(szBuffer, strlen(szBuffer));
	strHA2 = md5.toString();
	// ��ʼ���� response����qop�й�...
	if (m_strAuthQop.size() > 0) {
		// response = MD5(HA1:nonce:nonceCount:clientNonce:qop:HA2)
		sprintf(szBuffer, "%s:%s:%s:%s:%s:%s", strHA1.c_str(), m_strAuthNonce.c_str(), strNCount.c_str(), strCNonce.c_str(), m_strAuthQop.c_str(), strHA2.c_str());
	} else {
		// response=MD5(HA1:nonce:HA2)
		sprintf(szBuffer, "%s:%s:%s", strHA1.c_str(), m_strAuthNonce.c_str(), strHA2.c_str());
	}
	// ���㲢�������յ�responseֵ => md5��������...
	md5.reset(); md5.update(szBuffer, strlen(szBuffer));
	strResponse = md5.toString();
	// ���㲢����Authorization��HTTP��Ϣͷ����...
	sprintf(szBuffer, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", qop=%s, nc=%s, cnonce=\"%s\"", 
			theMapData["device_user"].c_str(), m_strAuthRealm.c_str(), m_strAuthNonce.c_str(), m_strCurlURI.c_str(),
			strResponse.c_str(), m_strAuthQop.c_str(), strNCount.c_str(), strCNonce.c_str());
	// ����curlͨ�õ�ISAPI�ӿڣ�����http״̬��...
	KT_HTTPStatusIndex nStatusIndex = httpOK;
	nStatusIndex = this->doCurlCommISAPI(szBuffer);
	// ��ӡ״̬��Ϣ���Ա���Ը���...
	blog(LOG_INFO, "curl ISAPI Auth => %d, %s", HTTPProtocol::GetStatusCode(nStatusIndex), HTTPProtocol::GetStatusCodeString(nStatusIndex).Ptr);
	// ���״̬�벻ΪΪOK��ֱ�ӷ���...
	if (nStatusIndex != httpOK)
		return false;
	ASSERT(nStatusIndex == httpOK);
	// ��¼��Ȩ�ɹ����趨״̬��־...
	m_bIsLoginISAPI = true;
	// ��һ��������ȡ����xml���ݰ�����...
	TiXmlDocument theXmlDoc;
	theXmlDoc.Parse(m_strUTF8Content.c_str());
	TiXmlElement * lpRootElem = theXmlDoc.RootElement();
	if (theXmlDoc.Error() || lpRootElem == NULL ) {
		blog(LOG_INFO, "xml error => %s", theXmlDoc.ErrorDesc());
		return false;
	}
	// ����xml�ɹ�֮��Ĵ���...
	TiXmlElement * lpZoomElem = lpRootElem->FirstChildElement("ContinuousZoomSpace");
	TiXmlElement * lpPanTiltElem = lpRootElem->FirstChildElement("ContinuousPanTiltSpace");
	TiXmlElement * lpXRange = ((lpPanTiltElem != NULL) ? lpPanTiltElem->FirstChildElement("XRange") : NULL);
	TiXmlElement * lpYRange = ((lpPanTiltElem != NULL) ? lpPanTiltElem->FirstChildElement("YRange") : NULL);
	TiXmlElement * lpZRange = ((lpZoomElem != NULL) ? lpZoomElem->FirstChildElement("ZRange") : NULL);

	// ��ȡ��̨������PTZ����������������Сֵ�����ֵ�������趨����...
	this->doParsePTZRange(lpXRange, m_XRange);
	this->doParsePTZRange(lpYRange, m_YRange);
	this->doParsePTZRange(lpZRange, m_ZRange);

	return true;
}

void CViewCamera::doParsePTZRange(TiXmlElement * lpXmlElem, POINT & outRange)
{
	TiXmlElement * lpMinElem = ((lpXmlElem != NULL) ? lpXmlElem->FirstChildElement("Min") : NULL);
	TiXmlElement * lpMaxElem = ((lpXmlElem != NULL) ? lpXmlElem->FirstChildElement("Max") : NULL);
	if (lpMinElem != NULL && lpMinElem->FirstChild() != NULL) {
		outRange.x = atoi(lpMinElem->FirstChild()->ToText()->Value());
	}
	if (lpMaxElem != NULL && lpMaxElem->FirstChild() != NULL) {
		outRange.y = atoi(lpMaxElem->FirstChild()->ToText()->Value());
	}
}

bool CViewCamera::doCameraStart()
{
	// ��������̶߳�����Ч��ֱ�ӷ���...
	if (m_lpDataThread != NULL)
		return true;
	// ��ȡ��ǰͨ�������ò���...
	GM_MapData theMapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	ASSERT(theMapData.size() > 0);
	// ��ȡ��ǰ�����������ͱ��...
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	// �ؽ��������������...
	ASSERT(m_lpDataThread == NULL);
	m_lpDataThread = new CRtspThread(this);
	// ��ʼ��ʧ�ܣ�ֱ�ӷ��� => ɾ������...
	if (!m_lpDataThread->InitThread()) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
		return false;
	}
	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 1 ���������ݽ��գ����ж�Ϊ���ӳ�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// ����ͨ��״̬Ϊ��������״̬...
	m_nCameraState = kCameraConnect;
	// ����������״̬��״̬ʱ�ӻ�û�����ã��������ʱ...
	this->update();
	// ״̬�㱨����Ҫ��rtsp���ӳɹ�֮����ܽ��� => CViewCamera::StartPushThread()
	return true;
}

bool CViewCamera::doCameraStop()
{
	// ֻ�е�ͨ�����������ɹ�״̬������Ҫ��ֹͣʱ��������㱨״̬...
	if (m_nCameraState >= kCameraOnLine) {
		// ����ת�������㱨ͨ����Ϣ��״̬...
		CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
		if (lpRemoteSession != NULL) {
			lpRemoteSession->doSendStopCameraCmd(m_nDBCameraID);
		}
		// ���ýӿ�֪ͨ������ => �޸�ͨ��״̬...
		CWebThread * lpWebThread = App()->GetWebThread();
		if (lpWebThread != NULL) {
			lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOffLine);
		}
	}
	// ɾ��UDP�����߳� => ����ʹ����...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// ɾ����Ƶ������������ => ����ʹ����...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
	// ֱ��ɾ�������̹߳�����...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// ����ͨ��״̬Ϊ����δ����״̬...
	m_nCameraState = kCameraOffLine;
	m_bIsLoginISAPI = false;
	// ������������صı�����Ϣ...
	m_nRecvKbps = 0;
	m_dwTimeOutMS = 0;
	m_nCurRecvByte = 0;
	// ����������״̬����������״̬������ǿ�Ƹ���...
	this->update();
	return true;
}

// ����˵���Ԥ���¼� => ������ر�SDL���Ŵ���...
void CViewCamera::doTogglePreviewShow()
{
	// ���ͨ��״̬������״̬С����Ҫɾ�����ػطţ�������Ԥ��״̬...
	if (m_nCameraState < kCameraOnLine) {
		m_bIsPreviewShow = false;
		this->doDeletePlayer();
		this->update();
		return;
	}
	// ͨ��״̬һ�����ڻ��������״̬...
	ASSERT(m_nCameraState >= kCameraOnLine);
	// �Ե�ǰ��Ԥ��״̬ȡ��...
	m_bIsPreviewShow = !m_bIsPreviewShow;
	// ���ݵ�ǰ״̬�Բ��Ŷ������ɾ�����ؽ�����...
	m_bIsPreviewShow ? this->doCreatePlayer() : this->doDeletePlayer();
	// ǿ�Ƹ��´��ڣ��ػ洰�ڻ���...
	this->update();
}

// ����˵���Ԥ�������¼� => ������ر���Ƶ����...
void CViewCamera::doTogglePreviewMute()
{
	pthread_mutex_lock(&m_MutexPlay);
	do {
		// ���û����Ƶ���Ŷ������þ���״̬...
		if (m_lpAudioPlay == NULL) {
			m_bIsPreviewMute = true;
			break;
		}
		// �����Ƶ���Ŷ�����Ч����Ҫ�Ծ�����־ȡ��...
		m_bIsPreviewMute = !m_bIsPreviewMute;
		// ���µľ���״̬���ø����Ŷ���...
		m_lpAudioPlay->SetMute(m_bIsPreviewMute);
	} while (false);
	pthread_mutex_unlock(&m_MutexPlay);
}

// �ؽ�����Ƶ�����̶߳���...
void CViewCamera::doCreatePlayer()
{
	// ���ȣ��ͷ�֮ǰ����������Ƶ���Ŷ���...
	this->doDeletePlayer();
	// ���뻥����󣬴����µ�����Ƶ���Ŷ���...
	pthread_mutex_lock(&m_MutexPlay);
	// ����ϵͳ0��ʱ�̵㣬Ϊ������Ƶͬ��...
	m_sys_zero_ns = os_gettime_ns();
	// ��ȡ��Ƶ���Ų�����������Ƶ���Ŷ���...
	if (m_lpDataThread->GetAACHeader().size() > 0) {
		int nRateIndex = m_lpDataThread->GetAudioRateIndex();
		int nChannelNum = m_lpDataThread->GetAudioChannelNum();
		m_lpAudioPlay = new CAudioPlay(this, m_sys_zero_ns);
		if (!m_lpAudioPlay->doInitAudio(nRateIndex, nChannelNum)) {
			delete m_lpAudioPlay; m_lpAudioPlay = NULL;
		}
		// �����Ĭ�Ͼ���״̬��������Ƶ������Ч�����þ����ӿ�...
		if (m_bIsPreviewMute && m_lpAudioPlay != NULL) {
			m_lpAudioPlay->SetMute(true);
		}
	}
	// ������Ƶ��Ⱦ�طŴ��ڶ��� => Ĭ�ϴ�������״̬...
	m_lpViewPlayer = new CViewRender(QString(""), NOTICE_FONT_HEIGHT, this);
	m_lpViewPlayer->raise();
	m_lpViewPlayer->show();
	// ��ȡ��Ƶ���Ų�����������Ƶ���Ŷ���...
	if (m_lpDataThread->GetAVCHeader().size() > 0) {
		int nVideoFPS = m_lpDataThread->GetVideoFPS();
		int nVideoWidth = m_lpDataThread->GetVideoWidth();
		int nVideoHeight = m_lpDataThread->GetVideoHeight();
		string & strSPS = m_lpDataThread->GetVideoSPS();
		string & strPPS = m_lpDataThread->GetVideoPPS();
		m_lpVideoPlay = new CVideoPlay(m_lpViewPlayer, m_sys_zero_ns);
		if (!m_lpVideoPlay->doInitVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS)) {
			delete m_lpVideoPlay; m_lpVideoPlay = NULL;
		}
	}
	// �˳����Ᵽ������...
	pthread_mutex_unlock(&m_MutexPlay);
}

// ɾ������Ƶ�����̶߳���...
void CViewCamera::doDeletePlayer()
{
	pthread_mutex_lock(&m_MutexPlay);
	// ɾ����Ƶ���Ŷ���...
	if (m_lpAudioPlay != NULL) {
		delete m_lpAudioPlay;
		m_lpAudioPlay = NULL;
	}
	// ɾ����Ƶ���Ŷ���...
	if (m_lpVideoPlay != NULL) {
		delete m_lpVideoPlay;
		m_lpVideoPlay = NULL;
	}
	// ɾ����Ƶ�طŴ��ڶ���...
	if (m_lpViewPlayer != NULL) {
		delete m_lpViewPlayer;
		m_lpViewPlayer = NULL;
	}
	// ����ϵͳ0��ʱ�̵�...
	m_sys_zero_ns = -1;
	m_start_pts_ms = -1;
	// �����ҵ���һ���ؼ�֡��־...
	m_bFindFirstVKey = false;
	m_bIsPreviewMute = true;
	// �˳����Ᵽ������...
	pthread_mutex_unlock(&m_MutexPlay);
}

// ����ȡ��������Ƶ���ݽ��б��ؽ���ط�...
void CViewCamera::doPushPlayer(FMS_FRAME & inFrame)
{
	// ���뻥�Ᵽ������...
	pthread_mutex_lock(&m_MutexPlay);
	// ���ϵͳ0��ʱ����Ч���ŵ����Ᵽ���嵱��...
	if (m_sys_zero_ns < 0 || m_lpViewPlayer == NULL) {
		pthread_mutex_unlock(&m_MutexPlay);
		return;
	}
	ASSERT(m_sys_zero_ns > 0 && m_lpViewPlayer != NULL);
	// ��ȡ��һ֡��PTSʱ��� => ��Ϊ����ʱ�����ע�ⲻ��ϵͳ0��ʱ��...
	uint32_t inSendTime = inFrame.dwSendTime;
	bool bIsKeyFrame = inFrame.is_keyframe;
	int inTypeTag = inFrame.typeFlvTag;
	if (m_start_pts_ms < 0) {
		m_start_pts_ms = inSendTime;
		blog(LOG_INFO, "%s StartPTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}
	// �����ǰ֡��ʱ����ȵ�һ֡��ʱ�����ҪС����Ҫ�ӵ������ó�����ʱ����Ϳ�����...
	if (inSendTime < m_start_pts_ms) {
		blog(LOG_INFO, "%s Error => SendTime: %lu, StartPTS: %I64d, Type: %d", TM_RECV_NAME, inSendTime, m_start_pts_ms, inTypeTag);
		inSendTime = m_start_pts_ms;
	}
	// ���㵱ǰ֡��ʱ��� => ʱ�����������������������...
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts_ms;

	// ע�⣺Ѱ�ҵ�һ����Ƶ�ؼ�֡��ʱ����Ƶ֡��Ҫ����...
	// �������Ƶ����Ƶ��һ֡��������Ƶ�ؼ�֡���������Ļ������ʧ��...
	if ((inTypeTag == PT_TAG_VIDEO) && (m_lpVideoPlay != NULL) && (!m_bFindFirstVKey)) {
		// �����ǰ��Ƶ֡�����ǹؼ�֡��ֱ�Ӷ���...
		if (!bIsKeyFrame) {
			//blog(LOG_INFO, "%s Discard for First Video KeyFrame => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
			m_lpViewPlayer->doUpdateNotice(QString(QTStr("Player.Window.DropVideoFrame")).arg(nCalcPTS));
			pthread_mutex_unlock(&m_MutexPlay);
			return;
		}
		// �����Ѿ��ҵ���һ����Ƶ�ؼ�֡��־...
		m_bFindFirstVKey = true;
		m_lpViewPlayer->doUpdateNotice(QTStr("Player.Window.FindFirstVKey"), true);
		blog(LOG_INFO, "%s Find First Video KeyFrame OK => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}

	// �����Ƶ�����߳���Ч����������Ƶ���ݰ����Ž�����Ƶ���ݰ�Ͷ�ݲ���...
	if (m_lpAudioPlay != NULL && inTypeTag == PT_TAG_AUDIO) {
		m_lpAudioPlay->doPushFrame(inFrame, nCalcPTS);
	}
	// �����Ƶ�����߳���Ч����������Ƶ���ݰ����Ž�����Ƶ���ݰ�Ͷ�ݲ���...
	if (m_lpVideoPlay != NULL && inTypeTag == PT_TAG_VIDEO) {
		m_lpVideoPlay->doPushFrame(inFrame, nCalcPTS);
	}
	// �ͷŻ��Ᵽ������...
	pthread_mutex_unlock(&m_MutexPlay);
}

// ��Ӧϵͳ��ļ����¼� => �����ķŴ����С�¼�...
bool CViewCamera::doVolumeEvent(int inKeyItem)
{
	// �����ǰ���ڲ��ǽ��㴰�ڣ�����ʧ��...
	if (!this->IsFoucs())
		return false;
	// ���ݰ����������Ŵ����С״̬...
	bool bResult = false;
	bool bIsVolPlus = false;
	switch (inKeyItem)
	{
	case Qt::Key_Less:  bIsVolPlus = false; break;
	case Qt::Key_Minus: bIsVolPlus = false; break;
	case Qt::Key_Equal: bIsVolPlus = true;  break;
	case Qt::Key_Plus:  bIsVolPlus = true;  break;
	default:			return false;
	}
	// ������Ƶ��������¼�Ͷ��...
	pthread_mutex_lock(&m_MutexPlay);
	if (m_lpAudioPlay != NULL) {
		bResult = m_lpAudioPlay->doVolumeEvent(bIsVolPlus);
	}
	pthread_mutex_unlock(&m_MutexPlay);
	return bResult;
}
