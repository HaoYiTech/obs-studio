
#include "window-login-mini.h"
#include "obs-app.hpp"
#include "FastSession.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMouseEvent>
#include <QPainter>
#include <QBitmap>
#include <QMovie>

CLoginMini::CLoginMini(QWidget *parent)
  : QDialog(parent)
  , m_lpMovieGif(NULL)
  , m_lpLoadBack(NULL)
  , m_nOnLineTimer(-1)
  , m_nTcpSocketFD(-1)
  , m_nCenterTimer(-1)
  , m_nCenterTcpPort(0)
  , m_CenterSession(NULL)
  , ui(new Ui::LoginMini)
  , m_eLoginCmd(kNoneCmd)
  , m_eMiniState(kCenterAddr)
{
	ui->setupUi(this);
	this->initWindow();
}

CLoginMini::~CLoginMini()
{
	this->killTimer(m_nCenterTimer);
	this->killTimer(m_nOnLineTimer);
	if (m_lpMovieGif != NULL) {
		delete m_lpMovieGif;
		m_lpMovieGif = NULL;
	}
	if (m_lpLoadBack != NULL) {
		delete m_lpLoadBack;
		m_lpLoadBack = NULL;
	}
	if (m_CenterSession != NULL) {
		delete m_CenterSession;
		m_CenterSession = NULL;
	}
}

void CLoginMini::loadStyleSheet(const QString &sheetName)
{
	QFile file(sheetName);
	file.open(QFile::ReadOnly);
	if (file.isOpen())
	{
		QString styleSheet = this->styleSheet();
		styleSheet += QLatin1String(file.readAll());
		this->setStyleSheet(styleSheet);
	}
}

// 绘制圆角窗口背景 => 由于是顶层窗口，需要使用蒙板...
void CLoginMini::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QPainterPath pathBack;

	// 更新整个背景颜色 => 圆角模式 => WA_TranslucentBackground
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	pathBack.addRoundedRect(QRect(0, 0, this->width(), this->height()), 4, 4);
	painter.fillPath(pathBack, QBrush(QColor(0, 122, 204)));
	// 显示小程序二维码...
	if (!m_QPixQRCode.isNull() && m_QPixQRCode.width() > 0) {
		int nXPos = (this->size().width() - m_QPixQRCode.size().width()) / 2;
		int nYPos = ui->hori_title->totalSizeHint().height();
		painter.drawPixmap(nXPos, nYPos, m_QPixQRCode);
	}
	// 显示版本|扫码|警告...
	ui->titleVer->setText(m_strVer);
	ui->titleScan->setText(m_strScan);
	ui->titleQR->setText(m_strQRNotice);

	QWidget::paintEvent(event);
}

void CLoginMini::initWindow()
{
	// FramelessWindowHint属性设置窗口去除边框;
	// WindowMinimizeButtonHint 属性设置在窗口最小化时，点击任务栏窗口可以显示出原窗口;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// 设置窗口背景透明 => 设置之后会造成全黑问题 => 顶层窗口需要用背景蒙板，普通窗口没问题...
	this->setAttribute(Qt::WA_TranslucentBackground);
	// 关闭窗口时释放资源;
	this->setAttribute(Qt::WA_DeleteOnClose);
	// 重新设定窗口大小...
	this->resize(350, 420);
	// 加载PTZ窗口的层叠显示样式表...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// 初始化版本文字信息...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// 设置背景动画图片 => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	// 将动画移动到窗口居中显示...
	QRect rcRect = this->rect();
	QRect rcSize = m_lpMovieGif->frameRect();
	int nXPos = (rcRect.width() - rcSize.width()) / 2;
	int nYPos = (rcRect.height() - rcSize.height()) / 2 - ui->btnClose->height() * 2 + 5;
	m_lpLoadBack->move(nXPos, nYPos);
	// 关联点击最小化按钮|关闭按钮的信号槽事件...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	// 关联网络信号槽反馈结果事件...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// 发起获取中心服务器的TCP地址和端口的命令...
	this->doWebGetCenterAddr();
}

void CLoginMini::doWebGetCenterAddr()
{
	// 中心会话对象有效，需要重建之...
	if (m_CenterSession != NULL) {
		delete m_CenterSession;
		m_CenterSession = NULL;
	}
	// 显示加载动画...
	m_lpLoadBack->show();
	// 删除中心连接检测时钟对象...
	if (m_nCenterTimer != -1) {
		this->killTimer(m_nCenterTimer);
		m_nCenterTimer = -1;
	}
	// 删除中心在线心跳包检测对象...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// 修改获取中心服务器TCP地址状态...
	m_eMiniState = kCenterAddr;
	m_strQRNotice = QStringLiteral("正在获取中心服务器地址...");
	// 构造凭证访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Mini/getUDPCenter");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	lpNetReply = m_objNetManager.get(theQTNetRequest);
	// 更新显示界面内容...
	this->update();
}

void CLoginMini::doWebGetMiniToken()
{
	// 修改获取小程序token状态...
	m_eMiniState = kMiniToken;
	m_strQRNotice = QStringLiteral("正在获取访问凭证...");
	// 构造凭证访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Mini/getToken");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	lpNetReply = m_objNetManager.get(theQTNetRequest);
	//QString strContentVal = QString("gather_id=%1&miniName=%2").arg(5).arg("device");
	//theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	//theQTNetRequest.setUrl(QUrl(strRequestURL));
	//lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

void CLoginMini::doWebGetMiniQRCode()
{
	// 判断access_token与path是否有效...
	if (m_strMiniPath.size() <= 0 || m_strMiniToken.size() <= 0) {
		m_strQRNotice = QStringLiteral("获取访问凭证失败，无法获取小程序码");
		this->update();
		return;
	}
	// 修改获取小程序二维码状态...
	m_strQRNotice = QStringLiteral("正在获取小程序码...");
	m_eMiniState = kMiniQRCode;
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	// 准备需要的汇报数据 => POST数据包...
	Json::Value itemData;
	itemData["width"] = kQRCodeWidth;
	itemData["page"] = m_strMiniPath.c_str();
	itemData["scene"] = QString("teacher_%1").arg(m_nTcpSocketFD).toStdString();
	QString strContentVal = QString("%1").arg(itemData.toStyledString().c_str());
	QString strRequestURL = QString("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%1").arg(m_strMiniToken.c_str());
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

void CLoginMini::onReplyFinished(QNetworkReply *reply)
{
	// 如果发生网络错误，打印错误信息，跳出循环...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		m_strQRNotice = QString("%1, %2").arg(reply->error()).arg(reply->errorString());
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// 读取完整的网络请求返回的内容数据包...
	int nStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	blog(LOG_INFO, "QT Status Code => %d", nStatusCode);
	// 根据状态分发处理过程...
	switch (m_eMiniState) {
	case kCenterAddr: this->onProcCenterAddr(reply); break;
	case kMiniToken:  this->onProcMiniToken(reply); break;
	case kMiniQRCode: this->onProcMiniQRCode(reply); break;
	}
}

void CLoginMini::onProcCenterAddr(QNetworkReply *reply)
{
	Json::Value value;
	ASSERT(m_eMiniState == kCenterAddr);
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
	if (!this->parseJson(strData, value, false)) {
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// 解析json成功，保存UDPCenter的TCP地址和端口...
	m_strCenterTcpAddr = OBSApp::getJsonString(value["udpcenter_addr"]);
	m_nCenterTcpPort = atoi(OBSApp::getJsonString(value["udpcenter_port"]).c_str());
	// 发起连接中心服务器的会话对象...
	this->doTcpConnCenterAddr();
}

void CLoginMini::doTcpConnCenterAddr()
{
	// 修改连接中心服务器状态...
	m_eMiniState = kCenterConn;
	m_strQRNotice = QStringLiteral("正在连接中心服务器...");
	// 创建中心会话对象 => 接收小程序反馈...
	ASSERT(m_CenterSession == NULL);
	m_CenterSession = new CCenterSession();
	m_CenterSession->InitSession(m_strCenterTcpAddr.c_str(), m_nCenterTcpPort);
	// 注册相关的事件反馈函数 => 通过信号槽消息绑定...
	this->connect(m_CenterSession, SIGNAL(doTriggerTcpConnect()), this, SLOT(onTriggerTcpConnect()));
	// 开启一个定时重建检测时钟 => 每隔5秒执行一次...
	m_nCenterTimer = this->startTimer(5 * 1000);
	// 更新显示界面内容...
	this->update();
}

// 响应中心会话对象的事件通知信号槽...
void CLoginMini::onTriggerTcpConnect()
{
	// 连接失败，打印错误信息...
	ASSERT(m_CenterSession != NULL);
	if (!m_CenterSession->IsConnected()) {
		m_strQRNotice = QString("%1%2").arg(QStringLiteral("连接中心服务器失败，错误号：")).arg(m_CenterSession->GetErrCode());
		blog(LOG_INFO, "QT error => %d", m_CenterSession->GetErrCode());
		m_eMiniState = kCenterAddr;
		m_lpLoadBack->hide();
		m_strScan.clear();
		this->update();
		return;
	}
	// 如果不是正在连接中心服务器状态，直接返回...
	if (m_eMiniState != kCenterConn)
		return;
	ASSERT(m_eMiniState == kCenterConn);
	// 将中心会话在服务器上的套接字编号进行保存...
	m_nTcpSocketFD = m_CenterSession->GetTcpSocketFD();
	// 每隔30秒检测一次，讲师端在中心服务器上在线汇报通知...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// 发起获取小程序Token值的网络命令...
	this->doWebGetMiniToken();
}

void CLoginMini::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nCenterTimer) {
		this->doCheckSession();
	} else if (nTimerID == m_nOnLineTimer) {
		this->doCheckOnLine();
	}
}

void CLoginMini::doCheckSession()
{
	// 如果中心会话对象无效或断开，发起获取中心服务器的TCP地址和端口的命令...
	if (m_CenterSession == NULL || !m_CenterSession->IsConnected()) {
		this->doWebGetCenterAddr();
	}
}

bool CLoginMini::doCheckOnLine()
{
	if (m_CenterSession == NULL)
		return false;
	return m_CenterSession->doSendOnLineCmd();
}

void CLoginMini::onProcMiniToken(QNetworkReply *reply)
{
	Json::Value value;
	ASSERT(m_eMiniState == kMiniToken);
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	// 如果是获取token状态，解析获取到json数据...
	blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
	if (!this->parseJson(strData, value, false)) {
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// 解析json成功，保存token与路径...
	m_strMiniToken = OBSApp::getJsonString(value["access_token"]);
	m_strMiniPath = OBSApp::getJsonString(value["mini_path"]);
	// 发起获取小程序二维码的操作...
	this->doWebGetMiniQRCode();
}

void CLoginMini::onProcMiniQRCode(QNetworkReply *reply)
{
	// 无论对错，都要关闭加载动画...
	m_lpLoadBack->hide();
	// 读取获取到的网络数据内容...
	ASSERT(m_eMiniState == kMiniQRCode);
	QByteArray & theByteArray = reply->readAll();
	// 从网路数据直接构造二维码的图片信息|设定默认扫码信息...
	if (m_QPixQRCode.loadFromData(theByteArray)) {
		m_strScan = QStringLiteral("请用微信扫码登录");
		m_strQRNotice.clear();
		this->update();
		return;
	}
	// 解除位图显示对象...
	Json::Value value;
	m_QPixQRCode.detach();
	// 构造二维码图片对象失败，进行错误解析...
	string & strData = theByteArray.toStdString();
	if (!this->parseJson(strData, value, true)) {
		this->update();
		return;
	}
	// 解析json数据包仍然失败，显示特定的错误信息...
	m_strQRNotice = QStringLiteral("解析获取到的JSON数据失败");
	this->update();
}

bool CLoginMini::parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin)
{
	Json::Reader reader;
	if (!reader.parse(inData, outValue)) {
		m_strQRNotice = QStringLiteral("解析获取到的JSON数据失败");
		return false;
	}
	const char * lpszCode = bIsWeiXin ? "errcode" : "err_code";
	const char * lpszMsg = bIsWeiXin ? "errmsg" : "err_msg";
	if (outValue[lpszCode].asBool()) {
		string & strMsg = OBSApp::getJsonString(outValue[lpszMsg]);
		string & strCode = OBSApp::getJsonString(outValue[lpszCode]);
		m_strQRNotice = QString("%1, %2").arg(strCode.c_str()).arg(strMsg.c_str());
		return false;
	}
	return true;
}

void CLoginMini::onButtonCloseClicked()
{
	this->close();
}

void CLoginMini::onButtonMinClicked()
{
	this->showMinimized();
}

// 以下通过 mousePressEvent | mouseMoveEvent | mouseReleaseEvent 三个事件实现了鼠标拖动标题栏移动窗口的效果;
void CLoginMini::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		m_isPressed = true;
		m_startMovePos = event->globalPos();
	}
	return QWidget::mousePressEvent(event);
}

void CLoginMini::mouseMoveEvent(QMouseEvent *event)
{
	if (m_isPressed) {
		QPoint movePoint = event->globalPos() - m_startMovePos;
		QPoint widgetPos = this->pos() + movePoint;
		m_startMovePos = event->globalPos();
		this->move(widgetPos.x(), widgetPos.y());
	}
	return QWidget::mouseMoveEvent(event);
}

void CLoginMini::mouseReleaseEvent(QMouseEvent *event)
{
	m_isPressed = false;
	return QWidget::mouseReleaseEvent(event);
}
