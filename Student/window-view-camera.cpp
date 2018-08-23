
#include <QPainter>
#include "speex-aec.h"
#include "student-app.h"
#include "pull-thread.h"
#include "web-thread.h"
#include "FastSession.h"
#include "UDPSendThread.h"
#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define LINE_SPACE_PIXEL		6
#define LEFT_SPACE_PIXEL		20
#define RIGHT_SPACE_PIXEL		20
#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_LIVE_COLOR        QColor(255,140,0)
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)

CViewCamera::CViewCamera(QWidget *parent, int nDBCameraID)
  : OBSQTDisplay(parent, 0)
  , m_nCameraState(kCameraOffLine)
  , m_nDBCameraID(nDBCameraID)
  , m_lpUDPSendThread(NULL)
  , m_lpDataThread(NULL)
  , m_bIsPreview(false)
  , m_lpViewLeft(NULL)
  , m_lpSpeexAEC(NULL)
  , m_nCurRecvByte(0)
  , m_dwTimeOutMS(0)
  , m_nFlowTimer(-1)
  , m_nRecvKbps(0)
{
	ASSERT(m_nDBCameraID > 0 && parent != NULL);
	m_lpViewLeft = qobject_cast<CViewLeft*>(parent);
	ASSERT(parent != NULL && m_lpViewLeft != NULL);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ���ô��ڱ����������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
	// ע�⣺ֻ��QT�̵߳��в�������ʱ�Ӷ���...
	// ����һ����ʱ���ʱ�� => ÿ��1��ִ��һ��...
	m_nFlowTimer = this->startTimer(1 * 1000);
}

CViewCamera::~CViewCamera()
{
	// ���ʱ����Ч��ɾ��ʱ�Ӷ���...
	if (m_nFlowTimer >= 0) {
		killTimer(m_nFlowTimer);
		m_nFlowTimer = -1;
	}
	// ɾ����Ƶ������������ => ����ʹ����...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// ɾ��UDP�����߳� => ����ʹ����...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// ɾ��������ȡ�̶߳���...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// ����Լ������Ǹ����㴰�ڣ���Ҫ����...
	App()->doResetFocus(this);
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
	// ���滭��ʵ����Ⱦ��������...
	m_rcRenderRect = rcRect;
}

void CViewCamera::DrawStatusText()
{
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
		this->onTriggerUdpSendThread(false, m_nDBCameraID);
		// ������������...
		nSendKbps = 0;
	}
	// ���ؽ������ʵ��ַ�������...
	return QString("%1 Kbps").arg(nSendKbps);
}

void CViewCamera::onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID)
{
	// ��������ͷ��ű����뵱ǰ����һ��...
	if (m_lpDataThread == NULL || nDBCameraID != m_nDBCameraID) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CDataThread is NULL");
		return;
	}
	ASSERT(m_lpDataThread != NULL && nDBCameraID == m_nDBCameraID);
	// ���ͨ������״̬��������״̬��ֱ�ӷ���...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CameraState: %d", m_nCameraState);
		return;
	}
	// �����ͨ����ʼ�������� => ���������߳�...
	if (bIsStartCmd) {
		// ��������߳��Ѿ������ˣ�ֱ�ӷ���...
		if (m_lpUDPSendThread != NULL)
			return;
		ASSERT(m_lpUDPSendThread == NULL);
		// ����UDP�����̣߳�ʹ�÷��������ݹ����Ĳ���...
		int nRoomID = atoi(App()->GetRoomIDStr().c_str());
		string & strUdpAddr = App()->GetUdpAddr();
		int nUdpPort = App()->GetUdpPort();
		m_lpUDPSendThread = new CUDPSendThread(m_lpDataThread, nRoomID, nDBCameraID);
		// �����ʼ��UDP�����߳�ʧ�ܣ�ɾ���Ѿ������Ķ���...
		if (!m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort)) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	} else {
		// �����ͨ��ֹͣ�������� => ɾ�������߳�...
		if (m_lpUDPSendThread != NULL) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	}
}

void CViewCamera::doStartPushThread()
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
	// �ؽ�����ʼ����Ƶ������������...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// ��ȡ��Ƶ��صĸ�ʽͷ��Ϣ���Լ�������������...
	int nInRateIndex = m_lpDataThread->GetAudioRateIndex();
	int nInChannelNum = m_lpDataThread->GetAudioChannelNum();
	int nOutSampleRate = App()->GetAudioSampleRate();
	int nOutChannelNum = App()->GetAudioChannelNum();
	int nHornDelayMS = App()->GetSpeexHornDelayMS();
	int nSpeexFrameMS = App()->GetSpeexFrameMS();
	int nSpeexFilterMS = App()->GetSpeexFilterMS();
	// ��������ʼ��������������...
	m_lpSpeexAEC = new CSpeexAEC(this);
	// ��ʼ��ʧ�ܣ�ɾ��������������...
	if (!m_lpSpeexAEC->InitSpeex(nInRateIndex, nInChannelNum, nOutSampleRate, nOutChannelNum, nHornDelayMS, nSpeexFrameMS, nSpeexFilterMS)) {
		delete m_lpSpeexAEC; m_lpSpeexAEC = NULL;
		blog(LOG_INFO, "CSpeexAEC::InitSpeex() => Error");
	}
	// ����ͨ��״̬Ϊ����������״̬...
	m_nCameraState = kCameraOnLine;
}

void CViewCamera::doPushFrame(FMS_FRAME & inFrame)
{
	// ���ͨ����������״̬����������...
	if (m_nCameraState != kCameraOnLine)
		return;
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	// ���UDP�����߳���Ч�����ң������߳�û�з�������ӵ����������������...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// �������Ƶ����֡����Ҫ���л�������...
	if (m_lpSpeexAEC != NULL && inFrame.typeFlvTag == PT_TAG_AUDIO) {
		m_lpSpeexAEC->PushMicFrame(inFrame);
	}
}

void CViewCamera::doEchoCancel(void * lpBufData, int nBufSize)
{
	if (m_lpSpeexAEC == NULL)
		return;
	m_lpSpeexAEC->PushHornPCM(lpBufData, nBufSize);
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
	if (m_nCameraState == kCameraOnLine) {
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
	// ɾ����Ƶ������������ => ����ʹ����...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// ɾ��UDP�����߳� => ����ʹ����...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// ֱ��ɾ�������̹߳�����...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// ����ͨ��״̬Ϊ����δ����״̬...
	m_nCameraState = kCameraOffLine;
	// ������������صı�����Ϣ...
	m_nRecvKbps = 0;
	m_dwTimeOutMS = 0;
	m_nCurRecvByte = 0;
	// ����������״̬����������״̬������ǿ�Ƹ���...
	this->update();
	return true;
}

// ����˵���Ԥ���¼� => ������ر�SDL���Ŵ���...
void CViewCamera::doTogglePreview()
{
}