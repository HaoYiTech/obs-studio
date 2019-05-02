
#include <QNetworkRequest>
#include <QAuthenticator>
#include <QNetworkReply>
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
#include "StringParser.h"
#include "tinyxml.h"
#include <curl.h>
#include <md5.h>

#define MAX_PTZ_SPEED			8
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
#define XML_DECLARE_UTF8		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

CCmdItem::CCmdItem(CViewCamera * lpViewCamera, CMD_ISAPI inCmdISAPI, int inSpeedVal)
  : m_lpViewCamera(lpViewCamera)
  , m_nSpeedVal(inSpeedVal)
  , m_nCmdISAPI(inCmdISAPI)
  , m_lpNetReply(NULL)
{
	GM_MapData theMapData;
	ASSERT(m_lpViewCamera != NULL);
	App()->GetCamera(m_lpViewCamera->GetDBCameraID(), theMapData);
	// 如果是对IMAGE能力查询命令，构造后直接返回...
	if (inCmdISAPI == kIMAGE_CAPABILITY) {
		m_strRequestURL = QString("http://%1%2")
			.arg(theMapData["device_ip"].c_str())
			.arg("/ISAPI/Image/channels/1/capabilities");
		return;
	}
	// 如果是对PTZ能力查询命令，构造后直接返回...
	if (inCmdISAPI == kPTZ_CAPABILITY) {
		m_strRequestURL = QString("http://%1%2")
			.arg(theMapData["device_ip"].c_str())
			.arg("/ISAPI/PTZCtrl/channels/1/capabilities");
		return;
	}
	// 改进后的逻辑：只要有其它命令包，一定是已经成功登录...
	ASSERT(m_lpViewCamera->IsCameraLoginISAPI());
	// 无论是否登录成功，都进行命令包的构造...
	this->doUpdateCmdRequest();
}

void CCmdItem::doUpdateCmdRequest()
{
	GM_MapData theMapData;
	ASSERT(m_lpViewCamera != NULL);
	App()->GetCamera(m_lpViewCamera->GetDBCameraID(), theMapData);

	int nCalcStep  = 0;
	int inSpeedVal = this->m_nSpeedVal;
	int inCmdISAPI = this->m_nCmdISAPI;

	POINT & XRange = m_lpViewCamera->m_XRange;
	POINT & YRange = m_lpViewCamera->m_YRange;
	POINT & ZRange = m_lpViewCamera->m_ZRange;
	POINT & FRange = m_lpViewCamera->m_FRange;
	POINT & IRange = m_lpViewCamera->m_IRange;

	switch ( inCmdISAPI )
	{
	case kPTZ_X_PAN:
		nCalcStep = ((inSpeedVal == 0) ? 0 : ((inSpeedVal > 0) ? XRange.y : XRange.x)) * 1.0f / MAX_PTZ_SPEED * abs(inSpeedVal);
		m_strContentVal = QString("%1<PTZData><pan>%2</pan><tilt>%3</tilt></PTZData>\r\n").arg(XML_DECLARE_UTF8).arg(nCalcStep).arg(0);
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/PTZCtrl/channels/1/continuous");
		break;
	case kPTZ_Y_TILT:
		nCalcStep = ((inSpeedVal == 0) ? 0 : ((inSpeedVal > 0) ? YRange.y : YRange.x)) * 1.0f / MAX_PTZ_SPEED * abs(inSpeedVal);
		m_strContentVal = QString("%1<PTZData><pan>%2</pan><tilt>%3</tilt></PTZData>\r\n").arg(XML_DECLARE_UTF8).arg(0).arg(nCalcStep);
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/PTZCtrl/channels/1/continuous");
		break;
	case kPTZ_Z_ZOOM:
		nCalcStep = ((inSpeedVal == 0) ? 0 : ((inSpeedVal > 0) ? ZRange.y : ZRange.x)) * 1.0f / MAX_PTZ_SPEED * abs(inSpeedVal);
		m_strContentVal = QString("%1<PTZData><zoom>%2</zoom></PTZData>\r\n").arg(XML_DECLARE_UTF8).arg(nCalcStep);
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/PTZCtrl/channels/1/continuous");
		break;
	case kPTZ_F_FOCUS:
		nCalcStep = ((inSpeedVal == 0) ? 0 : ((inSpeedVal > 0) ? FRange.y : FRange.x)) * 1.0f / MAX_PTZ_SPEED * abs(inSpeedVal);
		m_strContentVal = QString("%1<FocusData><focus>%2</focus></FocusData>\r\n").arg(XML_DECLARE_UTF8).arg(nCalcStep);
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/System/Video/inputs/channels/1/focus");
		break;
	case kPTZ_I_IRIS:
		nCalcStep = ((inSpeedVal == 0) ? 0 : ((inSpeedVal > 0) ? IRange.y : IRange.x)) * 1.0f / MAX_PTZ_SPEED * abs(inSpeedVal);
		m_strContentVal = QString("%1<IrisData><iris>%2</iris></IrisData>\r\n").arg(XML_DECLARE_UTF8).arg(nCalcStep);
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/System/Video/inputs/channels/1/iris");
		break;
	case kIMAGE_FLIP:
		if (inSpeedVal <= 0) {
			m_strContentVal = QString("%1<ImageFlip><enabled>false</enabled></ImageFlip>\r\n").arg(XML_DECLARE_UTF8);
		} else {
			m_strContentVal = QString("%1<ImageFlip><enabled>true</enabled><ImageFlipStyle>CENTER</ImageFlipStyle></ImageFlip>\r\n").arg(XML_DECLARE_UTF8);
		}
		m_strRequestURL = QString("http://%1%2").arg(theMapData["device_ip"].c_str()).arg("/ISAPI/Image/channels/1/imageFlip");
		break;
	}
}

CCmdItem::~CCmdItem()
{
	if (m_lpNetReply != NULL) {
		m_lpNetReply->deleteLater();
	}
}

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
	// 初始化回音消除互斥对象和播放线程互斥对象...
	pthread_mutex_init_value(&m_MutexPlay);
	pthread_mutex_init_value(&m_MutexAEC);
	pthread_mutex_init_value(&m_MutexSend);
	// 信号槽建立后，需要注意保存QNetworkReply对象...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	connect(&m_objNetManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator*)), this, SLOT(onAuthRequest(QNetworkReply *, QAuthenticator*)));
}

CViewCamera::~CViewCamera()
{
	// 如果时钟有效，删除时钟对象...
	if (m_nFlowTimer >= 0) {
		killTimer(m_nFlowTimer);
		m_nFlowTimer = -1;
	}
	// 删除UDP推流线程 => 数据使用者...
	pthread_mutex_lock(&m_MutexSend);
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	pthread_mutex_unlock(&m_MutexSend);
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
	pthread_mutex_destroy(&m_MutexSend);
	// 释放还没有执行的命令队列...
	this->ClearAllCmd();
}

void CViewCamera::ClearAllCmd()
{
	GM_DeqCmd::iterator itorItem;
	for (itorItem = m_deqCmd.begin(); itorItem != m_deqCmd.end(); ++itorItem) {
		CCmdItem * lpCmdItem = (*itorItem);
		delete lpCmdItem; lpCmdItem = NULL;
	}
	m_deqCmd.clear();
}

void CViewCamera::onReplyFinished(QNetworkReply *reply)
{
	// 没有正在执行的网络命令，直接返回...
	if (m_deqCmd.size() <= 0)
		return;
	// 判断网络对象是否与正在执行的命令一致...
	CCmdItem * lpCmdItem = m_deqCmd.front();
	if (lpCmdItem->GetNetReply() != reply)
		return;
	CMD_ISAPI nCurCmd = lpCmdItem->GetCmdISAPI();
	// 注意：需要在全部执行完毕之后，才删除队列元素...
	do {
		// 如果发生网络错误，打印错误信息，跳出循环...
		if (reply->error() != QNetworkReply::NoError) {
			blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
			// 如果是能力查询，需要终止对能力查询的周期调用功能...
			if (nCurCmd == kIMAGE_CAPABILITY || nCurCmd == kPTZ_CAPABILITY) {
				m_bIsLoginISAPI = true;
			}
			break;
		}
		// 读取完整的网络请求返回的内容数据包...
		int nStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray & theByteArray = reply->readAll();
		string & strData = theByteArray.toStdString();
		// 打印获取的网络数据内容和状态码...
		blog(LOG_INFO, "QT Status Code => %d", nStatusCode);
		blog(LOG_DEBUG,"QT Reply Data => %s", strData.c_str());
		// 如果状态码不是有效的，并且是能力查询，需要终止对能力查询的周期调用功能...
		if ((nStatusCode != 200) && (nCurCmd == kIMAGE_CAPABILITY || nCurCmd == kPTZ_CAPABILITY)) {
			m_bIsLoginISAPI = true;
			break;
		}
		// 分发命令，并解析获取到的数据，不同的命令，解析的方式不同...
		this->doParseResult(lpCmdItem, strData);
	} while (false);
	// 必须在全部分析完成之后，才删除队列元素，避免重复发送初始化命令...
	m_deqCmd.pop_front();
	// 删除队列之后才设定状态，避免重复命令...
	m_bIsNetRunning = false;
	// 注意：即使发生网络错误，也要执行新命令...
	// 从队列当中找一个新命令进行执行...
	this->doFirstCmdItem();
	// 删除有效的队列对象...
	delete lpCmdItem;
}

void CViewCamera::doParseResult(CCmdItem * lpCmdItem, string & inXmlData)
{
	if (lpCmdItem == NULL || inXmlData.size() <= 0)
		return;
	TiXmlDocument theXmlDoc;
	theXmlDoc.Parse(inXmlData.c_str());
	TiXmlElement * lpRootElem = theXmlDoc.RootElement();
	if (theXmlDoc.Error() || lpRootElem == NULL) {
		blog(LOG_INFO, "xml error => %s", theXmlDoc.ErrorDesc());
		return;
	}
	// 如果是对IMAGE能力查询结果反馈，需要进一步解析...
	if (lpCmdItem->GetCmdISAPI() == kIMAGE_CAPABILITY) {
		TiXmlElement * lpFlipElem = lpRootElem->FirstChildElement("ImageFlip");
		TiXmlElement * lpEnableElem = ((lpFlipElem != NULL) ? lpFlipElem->FirstChildElement("enabled") : NULL);
		TiXmlElement * lpStyleElem = ((lpFlipElem != NULL) ? lpFlipElem->FirstChildElement("ImageFlipStyle") : NULL);
		m_ImageFilpEnableVal = ((lpEnableElem != NULL) ? lpEnableElem->FirstChild()->ToText()->Value() : "");
		m_ImageFilpEnableOpt = ((lpEnableElem != NULL) ? lpEnableElem->Attribute("opt") : "");
		m_ImageFilpStyle = ((lpStyleElem != NULL) ? lpStyleElem->Attribute("opt") : "");
		App()->doUpdatePTZ(this->GetDBCameraID());
	}
	// 如果是对PTZ能力查询结果反馈，需要进一步解析...
	if (lpCmdItem->GetCmdISAPI() == kPTZ_CAPABILITY) {
		TiXmlElement * lpZoomElem = lpRootElem->FirstChildElement("ContinuousZoomSpace");
		TiXmlElement * lpPanTiltElem = lpRootElem->FirstChildElement("ContinuousPanTiltSpace");
		TiXmlElement * lpXRange = ((lpPanTiltElem != NULL) ? lpPanTiltElem->FirstChildElement("XRange") : NULL);
		TiXmlElement * lpYRange = ((lpPanTiltElem != NULL) ? lpPanTiltElem->FirstChildElement("YRange") : NULL);
		TiXmlElement * lpZRange = ((lpZoomElem != NULL) ? lpZoomElem->FirstChildElement("ZRange") : NULL);
		// 获取云台操作的PTZ三个向量步进的最小值和最大值，便于设定精度...
		this->doParsePTZRange(lpXRange, m_XRange);
		this->doParsePTZRange(lpYRange, m_YRange);
		this->doParsePTZRange(lpZRange, m_ZRange);
		// 由于焦距和光圈没有找到配置，使用默认配置...
		m_FRange.x = m_IRange.x = -100;
		m_FRange.y = m_IRange.y = +100;
		// 数据解析完毕，进行登录状态的修改...
		m_bIsLoginISAPI = true;
		// 注意：新的逻辑不需要更新其它命令包，因登录成功之前，其它命令包被阻止发送...
	}
	// 如果是图像翻转命令，需要对结果进行保存...
	if (lpCmdItem->GetCmdISAPI() == kIMAGE_FLIP) {
		m_ImageFilpEnableVal = ((lpCmdItem->GetSpeedVal() <= 0) ? "false" : "true");
	}
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

// 初始化登录ISAPI，成功之前doPTZCmd都会失败...
void CViewCamera::doCameraLoginISAPI()
{
	// 如果通道处于离线状态，直接返回...
	if (this->IsCameraOffLine())
		return;
	// 如果已经完成登录初始化，直接返回...
	if (this->IsCameraLoginISAPI())
		return;
	// 如果队列当中还有正在处理的命令，不能发送新命令...
	if (this->m_deqCmd.size() > 0)
		return;
	// 注意：后续还会根据需要，新增很多能力查询命令，以便后续配置...
	// 既没有能力查询命令也没有成功登录，这时才需要构造能力查询命令...
	CCmdItem * lpCmdItem = NULL;
	m_bIsNetRunning = false;
	// 发起图像能力查询命令...
	lpCmdItem = new CCmdItem(this, kIMAGE_CAPABILITY, 0);
	m_deqCmd.push_back(lpCmdItem);
	// 发起PTZ能力查询命令...
	lpCmdItem = new CCmdItem(this, kPTZ_CAPABILITY, 0);
	m_deqCmd.push_back(lpCmdItem);
	// 从队列中提起第一个命令并执行...
	this->doFirstCmdItem();
}

void CViewCamera::onAuthRequest(QNetworkReply *reply, QAuthenticator *authenticator)
{
	// 没有正在执行的网络命令，直接返回...
	if (m_deqCmd.size() <= 0)
		return;
	// 判断网络对象是否与正在执行的命令一致...
	CCmdItem * lpCmdItem = m_deqCmd.front();
	if (lpCmdItem->GetNetReply() != reply)
		return;
	// 网络对象一致，进行授权...
	GM_MapData theMapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	authenticator->setUser(theMapData["device_user"].c_str());
	authenticator->setPassword(theMapData["device_pass"].c_str());
}

bool CViewCamera::doPTZCmd(CMD_ISAPI inCMD, int inSpeedVal)
{
	// 如果通道没有登录成功，不能进行云台控制...
	if (!this->IsCameraLoginISAPI())
		return false;
	// 创建新的命令对象，并加入命令队列当中...
	CCmdItem * lpCmdItem = new CCmdItem(this, inCMD, inSpeedVal);
	m_deqCmd.push_back(lpCmdItem);
	// 从队列中提起第一个命令并执行...
	return this->doFirstCmdItem();
}

bool CViewCamera::doFirstCmdItem()
{
	// 队列为空，直接返回...
	if (m_deqCmd.size() <= 0)
		return false;
	// 如果有命令正在执行，直接返回...
	if (m_bIsNetRunning)
		return false;
	// 提取队列里第一个命令对象...
	CCmdItem * lpCmdItem = m_deqCmd.front();
	// 如果是能力查询对象，使用GET请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	CMD_ISAPI theCmdISAPI = lpCmdItem->GetCmdISAPI();
	QString & strRequestURL = lpCmdItem->GetRequestURL();
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	blog(LOG_INFO, "QT HTTP URL => %s", strRequestURL.toStdString().c_str());
	// 注意：可能有多种能力查询命令，是一个范围...
	if (theCmdISAPI <= kPTZ_CAPABILITY) {
		lpNetReply = m_objNetManager.get(theQTNetRequest);
		lpCmdItem->SetNetReply(lpNetReply);
		m_bIsNetRunning = true;
		return true;
	}

	// 注意：只有登录成功才能发起其它命令，这里判断就没有必要了...
	// 如果是其它命令，登录没有完成，不能进行命令请求...
	//if (!this->IsCameraLoginISAPI())
	//	return false;

	ASSERT(this->IsCameraLoginISAPI());

	// 已经登录成功，可以执行命令，都是PUT请求...
	QString & strContentVal = lpCmdItem->GetContentVal();
	blog(LOG_INFO, "QT PUT Content => %s", strContentVal.toStdString().c_str());
	lpNetReply = m_objNetManager.put(theQTNetRequest, strContentVal.toUtf8());
	lpCmdItem->SetNetReply(lpNetReply);
	m_bIsNetRunning = true;
	return true;
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
	pthread_mutex_lock(&m_MutexSend);
	if (m_lpUDPSendThread != NULL) {
		painter.setPen(TITLE_LIVE_COLOR);
		QFontMetrics & fontMetrics = painter.fontMetrics();
		QString strLive = QTStr("Camera.Window.Live");
		nPosX = rcRect.width() - 2 * fontMetrics.width(strLive);
		painter.drawText(nPosX, nPosY, strLive);
	}
	pthread_mutex_unlock(&m_MutexSend);
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
	QString strUrlVal = QTStr("Camera.Window.None");
	pthread_mutex_lock(&m_MutexSend);
	if (m_lpUDPSendThread != NULL) {
		string & strServerAddr = m_lpUDPSendThread->GetServerAddrStr();
		QString strQServerStr = QString::fromUtf8(strServerAddr.c_str());
		int nServerPort = m_lpUDPSendThread->GetServerPortInt();
		strUrlVal = QString("udp://%1/%2").arg(strQServerStr).arg(nServerPort);
	}
	pthread_mutex_unlock(&m_MutexSend);
	return strUrlVal;
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
	pthread_mutex_lock(&m_MutexSend);
	int nSendKbps = ((m_lpUDPSendThread != NULL) ? m_lpUDPSendThread->GetSendTotalKbps() : 0);
	pthread_mutex_unlock(&m_MutexSend);
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
	// 通知回音消除对象进行重建操作...
	pthread_mutex_lock(&m_MutexAEC);
	if (m_lpWebrtcAEC != NULL) {
		m_lpWebrtcAEC->onUdpRecvThreadStop();
	}
	pthread_mutex_unlock(&m_MutexAEC);
	/*// 通知视频播放对象需要进行重建 => 右侧SDL的退出会对这里的SDL有影响...
	pthread_mutex_lock(&m_MutexPlay);
	if (m_lpVideoPlay != NULL) {
		m_lpVideoPlay->onUdpRecvThreadStop();
	}
	pthread_mutex_unlock(&m_MutexPlay);*/
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
	if (m_lpUDPSendThread != NULL) {
		blog(LOG_INFO, "onTriggerUdpSendThread => OK, CameraState: %d", m_nCameraState);
		return;
	}
	ASSERT(m_lpUDPSendThread == NULL);
	// 重建回音消除对象 => 通道必须在线...
	this->ReBuildWebrtcAEC();
	// 创建UDP发送线程对象 => 通道必须在线...
	this->BuildSendThread();
}

void CViewCamera::doUdpSendThreadStop()
{
	// 只有当推流线程有效才能修改通道状态为在线状态...
	pthread_mutex_lock(&m_MutexSend);
	if (m_lpUDPSendThread != NULL) {
		m_nCameraState = kCameraOnLine;
	}
	// 如果是通道停止推流命令 => 删除推流线程...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	pthread_mutex_unlock(&m_MutexSend);
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
	// 注意：废弃这个接口，通道状态直接通过udpserver存取...
	// 调用接口通知服务器 => 修改通道状态 => 重建成功之后再发送命令...
	/*CWebThread * lpWebThread = App()->GetWebThread();
	if (lpWebThread != NULL) {
		lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOnLine);
	}*/
	// 设置通道状态为已连接在线状态...
	m_nCameraState = kCameraOnLine;
}

// 这里必须用信号槽，关联到同一线程，避免QTSocket的多线程访问故障...
void CViewCamera::doReadyToRecvFrame()
{
	QMetaObject::invokeMethod(this, "onTriggerReadyToRecvFrame");
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
		pthread_mutex_lock(&m_MutexSend);
		if (inFrame.typeFlvTag == PT_TAG_VIDEO && m_lpUDPSendThread != NULL) {
			m_lpUDPSendThread->PushFrame(inFrame);
		}
		pthread_mutex_unlock(&m_MutexSend);
	}
}

// 把回音消除之后的AAC音频，返回给UDP发送对象当中...
void CViewCamera::doPushAudioAEC(FMS_FRAME & inFrame)
{
	// 如果互斥对象无效，直接返回...
	if (m_MutexSend == NULL)
		return;
	pthread_mutex_lock(&m_MutexSend);
	// 如果UDP推流线程有效，并且，推流线程没有发生严重拥塞，继续传递数据...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	pthread_mutex_unlock(&m_MutexSend);
}

// 把投递到扬声器的PCM音频，放入回音消除当中...
void CViewCamera::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	// 如果互斥对象无效，直接返回...
	if (m_MutexAEC == NULL)
		return;
	// 进入互斥状态，并推送到回音消除对象...
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
		// 注意：废弃这个接口，通道状态直接通过udpserver存取...
		// 调用接口通知服务器 => 修改通道状态...
		/*CWebThread * lpWebThread = App()->GetWebThread();
		if (lpWebThread != NULL) {
			lpWebThread->doWebStatCamera(m_nDBCameraID, kCameraOffLine);
		}*/
	}
	// 删除UDP推流线程 => 数据使用者...
	pthread_mutex_lock(&m_MutexSend);
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	pthread_mutex_unlock(&m_MutexSend);
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
	m_bIsLoginISAPI = false;
	// 重置与拉流相关的变量信息...
	m_nRecvKbps = 0;
	m_dwTimeOutMS = 0;
	m_nCurRecvByte = 0;
	// 必须删除播放对象...
	this->doDeletePlayer();
	m_bIsPreviewShow = false;
	// 这里必须更新状态，处于离线状态，必须强制更新...
	this->update();
	return true;
}

// 处理菜单的预览事件 => 开启或关闭SDL播放窗口...
void CViewCamera::doTogglePreviewShow()
{
	// 如果通道状态比在线状态小，需要删除本地回放，并重置预览状态...
	if (m_nCameraState < kCameraOnLine) {
		m_bIsPreviewShow = false;
		this->doDeletePlayer();
		this->update();
		return;
	}
	// 通道状态一定大于或等于在线状态...
	ASSERT(m_nCameraState >= kCameraOnLine);
	// 对当前的预览状态取反...
	m_bIsPreviewShow = !m_bIsPreviewShow;
	// 根据当前状态对播放对象进行删除或重建操作...
	m_bIsPreviewShow ? this->doCreatePlayer() : this->doDeletePlayer();
	// 强制更新窗口，重绘窗口画面...
	this->update();
}

// 处理菜单的预览静音事件 => 开启或关闭音频内容...
void CViewCamera::doTogglePreviewMute()
{
	pthread_mutex_lock(&m_MutexPlay);
	do {
		// 如果没有音频播放对象，设置静音状态...
		if (m_lpAudioPlay == NULL) {
			m_bIsPreviewMute = true;
			break;
		}
		// 如果音频播放对象有效，需要对静音标志取反...
		m_bIsPreviewMute = !m_bIsPreviewMute;
		// 将新的静音状态设置给播放对象...
		m_lpAudioPlay->SetMute(m_bIsPreviewMute);
	} while (false);
	pthread_mutex_unlock(&m_MutexPlay);
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
		// 如果是默认静音状态，并且音频对象有效，调用静音接口...
		if (m_bIsPreviewMute && m_lpAudioPlay != NULL) {
			m_lpAudioPlay->SetMute(true);
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
	m_bIsPreviewMute = true;
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

// 响应系统层的键盘事件 => 音量的放大或缩小事件...
bool CViewCamera::doVolumeEvent(int inKeyItem)
{
	// 如果当前窗口不是焦点窗口，返回失败...
	if (!this->IsFoucs())
		return false;
	// 根据按键解析出放大或缩小状态...
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
	// 根据音频对象进行事件投递...
	pthread_mutex_lock(&m_MutexPlay);
	if (m_lpAudioPlay != NULL) {
		bResult = m_lpAudioPlay->doVolumeEvent(bIsVolPlus);
	}
	pthread_mutex_unlock(&m_MutexPlay);
	return bResult;
}
