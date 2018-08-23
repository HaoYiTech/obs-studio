
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
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// 设置窗口标题栏字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
	// 注意：只有QT线程当中才能启动时钟对象...
	// 开启一个定时检测时钟 => 每隔1秒执行一次...
	m_nFlowTimer = this->startTimer(1 * 1000);
}

CViewCamera::~CViewCamera()
{
	// 如果时钟有效，删除时钟对象...
	if (m_nFlowTimer >= 0) {
		killTimer(m_nFlowTimer);
		m_nFlowTimer = -1;
	}
	// 删除音频回音消除对象 => 数据使用者...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// 删除UDP推流线程 => 数据使用者...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// 删除数据拉取线程对象...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// 如果自己就是那个焦点窗口，需要重置...
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
	// 计算实际的接收码率 => Kbps...
	m_nRecvKbps = (m_nCurRecvByte * 8) / 1024;
	m_nCurRecvByte = 0;
	// 通道不是离线(正在连接或已连接)，就需要更新界面...
	if (m_nCameraState != kCameraOffLine) {
		this->update();
	}
}

// 注意：更新时钟放在 CPushThread::timerEvent() => 每秒更新一次...
void CViewCamera::paintEvent(QPaintEvent *event)
{
	// 绘制标题栏内容...
	this->DrawTitleArea();
	// 填充除标题栏之外的窗口区域...
	this->DrawRenderArea();
	// 显示通道的文字状态信息...
	this->DrawStatusText();
	//QWidget::paintEvent(event);
}

void CViewCamera::mousePressEvent(QMouseEvent *event)
{
	// 鼠标左键或右键事件点击 => 处理焦点事件...
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
	// 获取整个窗口客户区矩形大小...
	QRect & rcRect = this->rect();
	// 填充标题栏矩形区背景色...
	painter.fillRect(1, 1, rcRect.width()-2, TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// 设置标题栏文字颜色...
	painter.setPen(TITLE_TEXT_COLOR);
	// 计算标题栏坐标 => 特别注意：是文字左下角坐标...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT) / 2 + 1;
	// 先获取摄像头的标题名称，再进行字符串重组...
	QString strTitleContent = App()->GetCameraQName(m_nDBCameraID);
	QString strTitleText = QString("%1%2 - %3").arg(QStringLiteral("ID：")).arg(m_nDBCameraID).arg(strTitleContent);
	painter.drawText(nPosX, nPosY, strTitleText);
	// 绘制在线LIVE文字信息 => UDP线程有效...
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
	// 绘制最大矩形边框...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// 注意：这里绘制矩形的起始坐标从-1开始，QSplitter的顶点坐标是(1,1)...
	// 绘制边框，设置画笔颜色，缩放矩形区域...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// 填充渲染区矩形框...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	painter.fillRect(rcRect, m_bkColor);
	// 注意：这里使用PS截图后放大确认大小...
	// 保存画面实际渲染矩形区域...
	m_rcRenderRect = rcRect;
}

void CViewCamera::DrawStatusText()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// 调整矩形区域和画笔颜色...
	QRect rcRect = this->rect();
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	painter.setPen(QPen(STATUS_TEXT_COLOR));
	QFontMetrics & fontMetrics = painter.fontMetrics();
	// 计算顶部间距...
	int nLineSpace = LINE_SPACE_PIXEL;
	int nTopSpace = (rcRect.height() - TITLE_FONT_HEIGHT * 5 - nLineSpace * 4) / 2 - 20;
	// 显示通道状态标题栏 => 特别注意：是文字左下角坐标...
	int xNamePos = rcRect.left() + LEFT_SPACE_PIXEL;
	int yNamePos = rcRect.top() + nTopSpace + TITLE_FONT_HEIGHT;
	QString strTitleName = QTStr("Camera.Window.StatName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示通道状态具体内容...
	int xDataPos = xNamePos + fontMetrics.width(strTitleName);
	int yDataPos = yNamePos;
	// 获取通道状态文字...
	QString strDataValue;
	switch(m_nCameraState)
	{
	case kCameraOffLine: strDataValue = QTStr("Camera.Window.OffLine"); break;
	case kCameraConnect: strDataValue = QTStr("Camera.Window.Connect"); break;
	case kCameraOnLine:  strDataValue = QTStr("Camera.Window.OnLine");  break;
	default:             strDataValue = QTStr("Camera.Window.OffLine"); break;
	}
	painter.drawText(xDataPos, yDataPos, strDataValue);
	// 显示拉流地址标题栏 => 注意是左下角坐标...
	strTitleName = QTStr("Camera.Window.PullName");
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 设置文字对齐与自动换行...
	QRect rcText;
	QTextOption txtOption(Qt::AlignLeft);
	txtOption.setWrapMode(QTextOption::WrapAnywhere);
	// 计算拉流地址显示坐标位置 => 注意是左下角坐标...
	rcText.setTop(yNamePos - TITLE_FONT_HEIGHT - 2);
	rcText.setLeft(xNamePos + fontMetrics.width(strTitleName));
	rcText.setRight(rcRect.right() - RIGHT_SPACE_PIXEL);
	rcText.setBottom(rcText.top() + TITLE_FONT_HEIGHT * 4);
	// 获取拉流地址的具体内容...
	strDataValue = App()->GetCameraPullUrl(m_nDBCameraID);
	// 显示拉流地址信息，并累加下一行显示位置...
	painter.drawText(rcText, strDataValue, txtOption);
	yNamePos += TITLE_FONT_HEIGHT * 4 + nLineSpace;
	// 显示接收码率的标题栏...
	strTitleName = QTStr("Camera.Window.PullRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示接收码率的具体内容...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetRecvPullRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// 显示推流地址标题栏...
	strTitleName = QTStr("Camera.Window.PushName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示推流地址具体内容...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetStreamPushUrl();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// 显示推送码率标题栏...
	strTitleName = QTStr("Camera.Window.PushRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示推送码率具体内容...
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
	// 首先判断数据线程是否已经结束了...
	if (this->IsDataFinished())
		return true;
	// 一直没有新数据到达，超过 1 分钟，则判定为超时...
	int nWaitMinute = 1;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}

QString CViewCamera::GetRecvPullRate()
{
	// 设置默认的拉流计算值...
	int nRecvKbps = m_nRecvKbps;
	// 如果状态不是离线(正在连接或已连接)，就需要判断是否超时，超时设置为 -1 ...
	if (m_nCameraState != kCameraOffLine) {
		nRecvKbps = (this->IsFrameTimeout() ? -1 : nRecvKbps);
	}
	// 如果码率为负数，需要停止通道，同时，更新菜单...
	if ( nRecvKbps < 0 ) {
		// 先停止通道...
		this->doCameraStop();
		// 如果焦点窗口还是当前窗口，需要更新工具栏菜单状态...
		if (m_lpViewLeft->GetFocusID() == m_nDBCameraID) {
			emit m_lpViewLeft->enableCameraStart(true);
			emit m_lpViewLeft->enableCameraStop(false);
		}
		// 重置接收码率...
		nRecvKbps = 0;
	}
	// 返回接收码率的字符串内容...
	return QString("%1 Kbps").arg(nRecvKbps);
}

QString CViewCamera::GetSendPushRate()
{
	// 获取发送码率的具体数字 => 直接从UDP发送线程当中获取 => 注意：默认值是0...
	int nSendKbps = ((m_lpUDPSendThread != NULL) ? m_lpUDPSendThread->GetSendTotalKbps() : 0);
	// 如果为负数，需要删除UDP推流线程对象...
	if (nSendKbps < 0) {
		// 先删除UDP推流线程对象...
		this->onTriggerUdpSendThread(false, m_nDBCameraID);
		// 重置推流码率...
		nSendKbps = 0;
	}
	// 返回接收码率的字符串内容...
	return QString("%1 Kbps").arg(nSendKbps);
}

void CViewCamera::onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID)
{
	// 输入摄像头编号必须与当前窗口一致...
	if (m_lpDataThread == NULL || nDBCameraID != m_nDBCameraID) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CDataThread is NULL");
		return;
	}
	ASSERT(m_lpDataThread != NULL && nDBCameraID == m_nDBCameraID);
	// 如果通道拉流状态不是在线状态，直接返回...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CameraState: %d", m_nCameraState);
		return;
	}
	// 如果是通道开始推流命令 => 创建推流线程...
	if (bIsStartCmd) {
		// 如果推流线程已经创建了，直接返回...
		if (m_lpUDPSendThread != NULL)
			return;
		ASSERT(m_lpUDPSendThread == NULL);
		// 创建UDP推流线程，使用服务器传递过来的参数...
		int nRoomID = atoi(App()->GetRoomIDStr().c_str());
		string & strUdpAddr = App()->GetUdpAddr();
		int nUdpPort = App()->GetUdpPort();
		m_lpUDPSendThread = new CUDPSendThread(m_lpDataThread, nRoomID, nDBCameraID);
		// 如果初始化UDP推流线程失败，删除已经创建的对象...
		if (!m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort)) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	} else {
		// 如果是通道停止推流命令 => 删除推流线程...
		if (m_lpUDPSendThread != NULL) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	}
}

void CViewCamera::doStartPushThread()
{
	// 向中转服务器汇报通道信息和状态 => 重建成功之后再发送命令...
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	if (lpRemoteSession != NULL) {
		lpRemoteSession->doSendStartCameraCmd(m_nDBCameraID);
	}
	// 调用接口通知服务器 => 修改通道状态 => 重建成功之后再发送命令...
	CWebThread * lpWebThread = App()->GetWebThread();
	if (lpWebThread != NULL) {
		lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOnLine);
	}
	// 重建并初始化音频回音消除对象...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// 获取音频相关的格式头信息，以及回音消除参数...
	int nInRateIndex = m_lpDataThread->GetAudioRateIndex();
	int nInChannelNum = m_lpDataThread->GetAudioChannelNum();
	int nOutSampleRate = App()->GetAudioSampleRate();
	int nOutChannelNum = App()->GetAudioChannelNum();
	int nHornDelayMS = App()->GetSpeexHornDelayMS();
	int nSpeexFrameMS = App()->GetSpeexFrameMS();
	int nSpeexFilterMS = App()->GetSpeexFilterMS();
	// 创建并初始化回音消除对象...
	m_lpSpeexAEC = new CSpeexAEC(this);
	// 初始化失败，删除回音消除对象...
	if (!m_lpSpeexAEC->InitSpeex(nInRateIndex, nInChannelNum, nOutSampleRate, nOutChannelNum, nHornDelayMS, nSpeexFrameMS, nSpeexFilterMS)) {
		delete m_lpSpeexAEC; m_lpSpeexAEC = NULL;
		blog(LOG_INFO, "CSpeexAEC::InitSpeex() => Error");
	}
	// 设置通道状态为已连接在线状态...
	m_nCameraState = kCameraOnLine;
}

void CViewCamera::doPushFrame(FMS_FRAME & inFrame)
{
	// 如果通道不是拉流状态，不能推流...
	if (m_nCameraState != kCameraOnLine)
		return;
	// 将超时计时点复位，重新计时...
	m_dwTimeOutMS = ::GetTickCount();
	// 累加接收数据包的字节数，加入缓存队列...
	m_nCurRecvByte += inFrame.strData.size();
	// 如果UDP推流线程有效，并且，推流线程没有发生严重拥塞，继续传递数据...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// 如果是音频数据帧，需要进行回音消除...
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
	// 如果数据线程对象有效，直接返回...
	if (m_lpDataThread != NULL)
		return true;
	// 获取当前通道的配置参数...
	GM_MapData theMapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	ASSERT(theMapData.size() > 0);
	// 获取当前的流数据类型编号...
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	// 重建推流对象管理器...
	ASSERT(m_lpDataThread == NULL);
	m_lpDataThread = new CRtspThread(this);
	// 初始化失败，直接返回 => 删除对象...
	if (!m_lpDataThread->InitThread()) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
		return false;
	}
	// 初始化接收数据超时时间记录起点 => 超过 1 分钟无数据接收，则判定为连接超时...
	m_dwTimeOutMS = ::GetTickCount();
	// 设置通道状态为正在连接状态...
	m_nCameraState = kCameraConnect;
	// 这里必须更新状态，状态时钟还没起作用，会造成延时...
	this->update();
	// 状态汇报，需要等rtsp连接成功之后才能进行 => CViewCamera::StartPushThread()
	return true;
}

bool CViewCamera::doCameraStop()
{
	// 只有当通道处于拉流成功状态，才需要在停止时向服务器汇报状态...
	if (m_nCameraState == kCameraOnLine) {
		// 向中转服务器汇报通道信息和状态...
		CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
		if (lpRemoteSession != NULL) {
			lpRemoteSession->doSendStopCameraCmd(m_nDBCameraID);
		}
		// 调用接口通知服务器 => 修改通道状态...
		CWebThread * lpWebThread = App()->GetWebThread();
		if (lpWebThread != NULL) {
			lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOffLine);
		}
	}
	// 删除音频回音消除对象 => 数据使用者...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// 删除UDP推流线程 => 数据使用者...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// 直接删除数据线程管理器...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// 设置通道状态为离线未连接状态...
	m_nCameraState = kCameraOffLine;
	// 重置与拉流相关的变量信息...
	m_nRecvKbps = 0;
	m_dwTimeOutMS = 0;
	m_nCurRecvByte = 0;
	// 这里必须更新状态，处于离线状态，必须强制更新...
	this->update();
	return true;
}

// 处理菜单的预览事件 => 开启或关闭SDL播放窗口...
void CViewCamera::doTogglePreview()
{
}