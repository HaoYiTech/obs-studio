
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

CViewCamera::CViewCamera(QWidget *parent, int nDBCameraID)
  : OBSQTDisplay(parent, 0)
  , m_nCameraState(kCameraOffLine)
  , m_nDBCameraID(nDBCameraID)
  , m_bFindFirstVKey(false)
  , m_lpUDPSendThread(NULL)
  , m_lpDataThread(NULL)
  , m_lpViewPlayer(NULL)
  , m_bIsPreview(false)
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
	// 保存父窗口对象...
	ASSERT(m_nDBCameraID > 0 && parent != NULL);
	m_lpViewLeft = qobject_cast<CViewLeft*>(parent);
	ASSERT(parent != NULL && m_lpViewLeft != NULL);
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	// 设置窗口标题栏字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
	// 注意：只有QT线程当中才能启动时钟对象...
	// 开启一个定时检测时钟 => 每隔1秒执行一次...
	m_nFlowTimer = this->startTimer(1 * 1000);
	// 建立摄像头数据拉取成功的信号槽，为了避免RTSP数据线程调用QT的socket造成的问题...
	this->connect(this, SIGNAL(doTriggerReadyToRecvFrame()), this, SLOT(onTriggerReadyToRecvFrame()));
	// 初始化回音消除互斥对象和播放线程互斥对象...
	pthread_mutex_init_value(&m_MutexPlay);
	pthread_mutex_init_value(&m_MutexAEC);
}

CViewCamera::~CViewCamera()
{
	// 如果时钟有效，删除时钟对象...
	if (m_nFlowTimer >= 0) {
		killTimer(m_nFlowTimer);
		m_nFlowTimer = -1;
	}
	// 删除UDP推流线程 => 数据使用者...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// 删除音频回音消除对象 => 数据使用者...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
	// 删除数据拉取线程对象...
	if (m_lpDataThread != NULL) {
		delete m_lpDataThread;
		m_lpDataThread = NULL;
	}
	// 如果自己就是那个焦点窗口，需要重置...
	App()->doResetFocus(this);
	// 释放音视频播放线程对象...
	this->doDeletePlayer();
	// 释放回音消除互斥对象和播放互斥对象...
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
	// 计算实际的接收码率 => Kbps...
	m_nRecvKbps = (m_nCurRecvByte * 8) / 1024;
	m_nCurRecvByte = 0;
	// 如果正在进行本地回放，需要手动更新，但不显示...
	if (m_lpViewPlayer != NULL) {
		this->GetRecvPullRate();
		this->GetSendPushRate();
		return;
	}
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
	// 绘画对象活动状态下才进行绘制...
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
	// 绘画对象活动状态下才进行绘制...
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
	// 渲染窗口有效，重置渲染窗口的矩形位置...
	if (m_lpViewPlayer != NULL) {
		m_lpViewPlayer->doResizeRect(rcRect);
	}
}

void CViewCamera::DrawStatusText()
{
	// 如果视频回放窗口有效，不要绘制文字...
	if (m_lpViewPlayer != NULL)
		return;
	ASSERT(m_lpViewPlayer == NULL);
	// 绘画对象活动状态下才进行绘制...
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
	case kCameraPusher:  strDataValue = QTStr("Camera.Window.OnLine");  break;
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
		this->doUdpSendThreadStop();
		// 重置推流码率...
		nSendKbps = 0;
	}
	// 返回接收码率的字符串内容...
	return QString("%1 Kbps").arg(nSendKbps);
}

// 处理右侧播放线程已经停止通知...
void CViewCamera::onUdpRecvThreadStop()
{
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		m_lpWebrtcAEC->onUdpRecvThreadStop();
	}
	pthread_mutex_unlock(&m_MutexAEC);
}

void CViewCamera::doUdpSendThreadStart()
{
	// 如果通道拉流状态不是在线状态，直接返回...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CameraState: %d", m_nCameraState);
		return;
	}
	// 如果摄像头数据线程无效，打印错误，返回...
	if (m_lpDataThread == NULL) {
		blog(LOG_INFO, "onTriggerUdpSendThread => Error, CDataThread is NULL");
		return;
	}
	// 如果推流线程已经创建了，直接返回...
	if (m_lpUDPSendThread != NULL)
		return;
	ASSERT(m_lpUDPSendThread == NULL);
	// 重建回音消除对象 => 通道必须在线...
	this->ReBuildWebrtcAEC();
	// 创建UDP发送线程对象 => 通道必须在线...
	this->BuildSendThread();
}

void CViewCamera::doUdpSendThreadStop()
{
	// 只有当推流线程有效才能修改通道状态为在线状态...
	if (m_lpUDPSendThread != NULL) {
		m_nCameraState = kCameraOnLine;
	}
	// 如果是通道停止推流命令 => 删除推流线程...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// 后删除回音消除对象...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
	// 更新界面，强制绘制标题栏...
	this->update();
}

void CViewCamera::BuildSendThread()
{
	// 通道必须是在线状态...
	if (m_nCameraState != kCameraOnLine) {
		blog(LOG_INFO, "== BuildSendThread Error for CameraState ==");
		return;
	}
	ASSERT(m_nCameraState == kCameraOnLine);
	// 这里webrtc的DA-AEC回音消除对象必须有效...
	if (m_lpWebrtcAEC == NULL) {
		blog(LOG_INFO, "== BuildSendThread Error for WebrtcAEC ==");
		return;
	}
	// 音频参数需要特殊处理，统一使用全局的音频设定，因为回音消除对象必须有效...
	int nAudioRateIndex = App()->GetAudioRateIndex();
	int nAudioChannelNum = App()->GetAudioChannelNum();
	// 创建UDP推流线程，使用服务器传递过来的参数...
	int nRoomID = atoi(App()->GetRoomIDStr().c_str());
	string & strUdpAddr = App()->GetUdpAddr();
	int nUdpPort = App()->GetUdpPort();
	int nDBCameraID = m_nDBCameraID;
	m_lpUDPSendThread = new CUDPSendThread(m_lpDataThread, nRoomID, nDBCameraID);
	// 如果初始化UDP推流线程失败，删除已经创建的对象...
	if (!m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort, nAudioRateIndex, nAudioChannelNum)) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
		return;
	}
	// 设置当前通道为正在推流状态...
	m_nCameraState = kCameraPusher;
	// 更新界面，强制绘制标题栏...
	this->update();
}

void CViewCamera::ReBuildWebrtcAEC()
{
	// 通道必须是在线状态...
	if (m_nCameraState != kCameraOnLine)
		return;
	ASSERT(m_nCameraState == kCameraOnLine);
	// 进入互斥保护对象...
	pthread_mutex_lock(&m_MutexAEC);
	// 重建并初始化音频回音消除对象...
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	// 获取音频相关的格式头信息，以及回音消除参数...
	int nInRateIndex = m_lpDataThread->GetAudioRateIndex();
	int nInChannelNum = m_lpDataThread->GetAudioChannelNum();
	int nOutSampleRate = App()->GetAudioSampleRate();
	int nOutChannelNum = App()->GetAudioChannelNum();
	int nOutBitrateAAC = App()->GetAudioBitrateAAC();
	// 创建并初始化回音消除对象...
	m_lpWebrtcAEC = new CWebrtcAEC(this);
	// 初始化失败，删除回音消除对象...
	if (!m_lpWebrtcAEC->InitWebrtc(nInRateIndex, nInChannelNum, nOutSampleRate, nOutChannelNum, nOutBitrateAAC)) {
		delete m_lpWebrtcAEC; m_lpWebrtcAEC = NULL;
		blog(LOG_INFO, "== CWebrtcAEC::InitWebrtc() => Error ==");
	}
	// 退出互斥保护对象...
	pthread_mutex_unlock(&m_MutexAEC);
}

// 信号doTriggerReadyToRecvFrame的执行函数...
void CViewCamera::onTriggerReadyToRecvFrame()
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
	// 设置通道状态为已连接在线状态...
	m_nCameraState = kCameraOnLine;
}

// 这里必须用信号槽，关联到同一线程，避免QTSocket的多线程访问故障...
void CViewCamera::doReadyToRecvFrame()
{
	emit this->doTriggerReadyToRecvFrame();
}

void CViewCamera::doPushFrame(FMS_FRAME & inFrame)
{
	// 如果通道比在线状态小，不能投递数据...
	if (m_nCameraState < kCameraOnLine)
		return;
	ASSERT(m_nCameraState >= kCameraOnLine);
	// 将获取到的音视频数据进行本地解码回放...
	this->doPushPlayer(inFrame);
	// 将超时计时点复位，重新计时...
	m_dwTimeOutMS = ::GetTickCount();
	// 累加接收数据包的字节数，加入缓存队列...
	m_nCurRecvByte += inFrame.strData.size();
	// 如果通道不是推流状态，不能进行数据发送...
	if (m_nCameraState != kCameraPusher)
		return;
	ASSERT(m_nCameraState == kCameraPusher);
	// 如果UDP推流线程有效，并且，推流线程没有发生严重拥塞，继续传递数据...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		// 如果是音频，回音消除对象必然有效，投递给回音消除对象...
		// 注意：必须投递给回音消除对象，因为音频格式要转换...
		if (inFrame.typeFlvTag == PT_TAG_AUDIO) {
			pthread_mutex_lock(&m_MutexAEC);
			if (m_lpWebrtcAEC != NULL) {
				m_lpWebrtcAEC->PushMicFrame(inFrame);
			}
			pthread_mutex_unlock(&m_MutexAEC);
		}
		// 如果是视频，投递到UDP发送线程，进行打包发送...
		if (inFrame.typeFlvTag == PT_TAG_VIDEO) {
			m_lpUDPSendThread->PushFrame(inFrame);
		}
	}
}

// 把回音消除之后的AAC音频，返回给UDP发送对象当中...
void CViewCamera::doPushAudioAEC(FMS_FRAME & inFrame)
{
	// 如果UDP推流线程有效，并且，推流线程没有发生严重拥塞，继续传递数据...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
}

// 把投递到扬声器的PCM音频，放入回音消除当中...
void CViewCamera::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		m_lpWebrtcAEC->PushHornPCM(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf);
	}
	pthread_mutex_unlock(&m_MutexAEC);
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
	if (m_nCameraState >= kCameraOnLine) {
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
	// 删除UDP推流线程 => 数据使用者...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	// 删除音频回音消除对象 => 数据使用者...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		delete m_lpWebrtcAEC;
		m_lpWebrtcAEC = NULL;
	}
	pthread_mutex_unlock(&m_MutexAEC);
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
	// 如果通道状态比在线状态小，需要删除本地回放，并重置预览状态...
	if (m_nCameraState < kCameraOnLine) {
		m_bIsPreview = false;
		this->doDeletePlayer();
		this->update();
		return;
	}
	// 通道状态一定大于或等于在线状态...
	ASSERT(m_nCameraState >= kCameraOnLine);
	// 对当前的预览状态取反...
	m_bIsPreview = !m_bIsPreview;
	// 根据当前状态对播放对象进行删除或重建操作...
	m_bIsPreview ? this->doCreatePlayer() : this->doDeletePlayer();
	// 强制更新窗口，重绘窗口画面...
	this->update();
}

// 重建音视频播放线程对象...
void CViewCamera::doCreatePlayer()
{
	// 首先，释放之前创建的音视频播放对象...
	this->doDeletePlayer();
	// 进入互斥对象，创建新的音视频播放对象...
	pthread_mutex_lock(&m_MutexPlay);
	// 重置系统0点时刻点，为了音视频同步...
	m_sys_zero_ns = os_gettime_ns();
	// 获取音频播放参数，创建音频播放对象...
	if (m_lpDataThread->GetAACHeader().size() > 0) {
		int nRateIndex = m_lpDataThread->GetAudioRateIndex();
		int nChannelNum = m_lpDataThread->GetAudioChannelNum();
		m_lpAudioPlay = new CAudioPlay(this, m_sys_zero_ns);
		if (!m_lpAudioPlay->doInitAudio(nRateIndex, nChannelNum)) {
			delete m_lpAudioPlay; m_lpAudioPlay = NULL;
		}
	}
	// 创建视频渲染回放窗口对象 => 默认处于隐藏状态...
	m_lpViewPlayer = new CViewRender(QString(""), NOTICE_FONT_HEIGHT, this);
	m_lpViewPlayer->raise();
	m_lpViewPlayer->show();
	// 获取视频播放参数，创建视频播放对象...
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
	// 退出互斥保护对象...
	pthread_mutex_unlock(&m_MutexPlay);
}

// 删除音视频播放线程对象...
void CViewCamera::doDeletePlayer()
{
	pthread_mutex_lock(&m_MutexPlay);
	// 删除音频播放对象...
	if (m_lpAudioPlay != NULL) {
		delete m_lpAudioPlay;
		m_lpAudioPlay = NULL;
	}
	// 删除视频播放对象...
	if (m_lpVideoPlay != NULL) {
		delete m_lpVideoPlay;
		m_lpVideoPlay = NULL;
	}
	// 删除视频回放窗口对象...
	if (m_lpViewPlayer != NULL) {
		delete m_lpViewPlayer;
		m_lpViewPlayer = NULL;
	}
	// 重置系统0点时刻点...
	m_sys_zero_ns = -1;
	m_start_pts_ms = -1;
	// 重置找到第一个关键帧标志...
	m_bFindFirstVKey = false;
	// 退出互斥保护对象...
	pthread_mutex_unlock(&m_MutexPlay);
}

// 将获取到的音视频数据进行本地解码回放...
void CViewCamera::doPushPlayer(FMS_FRAME & inFrame)
{
	// 进入互斥保护对象...
	pthread_mutex_lock(&m_MutexPlay);
	// 如果系统0点时刻无效，放到互斥保护体当中...
	if (m_sys_zero_ns < 0 || m_lpViewPlayer == NULL) {
		pthread_mutex_unlock(&m_MutexPlay);
		return;
	}
	ASSERT(m_sys_zero_ns > 0 && m_lpViewPlayer != NULL);
	// 获取第一帧的PTS时间戳 => 做为启动时间戳，注意不是系统0点时刻...
	uint32_t inSendTime = inFrame.dwSendTime;
	bool bIsKeyFrame = inFrame.is_keyframe;
	int inTypeTag = inFrame.typeFlvTag;
	if (m_start_pts_ms < 0) {
		m_start_pts_ms = inSendTime;
		blog(LOG_INFO, "%s StartPTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}
	// 如果当前帧的时间戳比第一帧的时间戳还要小，不要扔掉，设置成启动时间戳就可以了...
	if (inSendTime < m_start_pts_ms) {
		blog(LOG_INFO, "%s Error => SendTime: %lu, StartPTS: %I64d, Type: %d", TM_RECV_NAME, inSendTime, m_start_pts_ms, inTypeTag);
		inSendTime = m_start_pts_ms;
	}
	// 计算当前帧的时间戳 => 时间戳必须做修正，否则会混乱...
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts_ms;

	// 注意：寻找第一个视频关键帧的时候，音频帧不要丢弃...
	// 如果有视频，视频第一帧必须是视频关键帧，不丢弃的话解码会失败...
	if ((inTypeTag == PT_TAG_VIDEO) && (m_lpVideoPlay != NULL) && (!m_bFindFirstVKey)) {
		// 如果当前视频帧，不是关键帧，直接丢弃...
		if (!bIsKeyFrame) {
			//blog(LOG_INFO, "%s Discard for First Video KeyFrame => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
			m_lpViewPlayer->doUpdateNotice(QString(QTStr("Player.Window.DropVideoFrame")).arg(nCalcPTS));
			pthread_mutex_unlock(&m_MutexPlay);
			return;
		}
		// 设置已经找到第一个视频关键帧标志...
		m_bFindFirstVKey = true;
		m_lpViewPlayer->doUpdateNotice(QTStr("Player.Window.FindFirstVKey"), true);
		blog(LOG_INFO, "%s Find First Video KeyFrame OK => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}

	// 如果音频播放线程有效，并且是音频数据包，才进行音频数据包投递操作...
	if (m_lpAudioPlay != NULL && inTypeTag == PT_TAG_AUDIO) {
		m_lpAudioPlay->doPushFrame(inFrame, nCalcPTS);
	}
	// 如果视频播放线程有效，并且是视频数据包，才进行视频数据包投递操作...
	if (m_lpVideoPlay != NULL && inTypeTag == PT_TAG_VIDEO) {
		m_lpVideoPlay->doPushFrame(inFrame, nCalcPTS);
	}
	// 释放互斥保护对象...
	pthread_mutex_unlock(&m_MutexPlay);
}
