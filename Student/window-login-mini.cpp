
#include "window-login-mini.h"
#include "win-update.hpp"
#include "student-app.h"
#include "FastSession.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMouseEvent>
#include <QPainter>
#include <QBitmap>
#include <QMovie>
#include <time.h>

#define UPDATE_CHECK_INTERVAL (60*60*24*4) /* 4 days */

void CLoginMini::TimedCheckForUpdates()
{
	// 检测是否开启自动更新开关，默认是处于开启状态 => CStudentApp::InitGlobalConfigDefaults()...
	if (!config_get_bool(App()->GlobalConfig(), "General", "EnableAutoUpdates"))
		return;
	// 注意：LastUpdateCheck只在用户点击“稍后提醒”才会起作用(4天后)；如果是相同版本，也会每次启动都会检测；
	long long lastUpdate = config_get_int(App()->GlobalConfig(), "General", "LastUpdateCheck");
	uint32_t lastVersion = config_get_int(App()->GlobalConfig(), "General", "LastVersion");
	// 如果上次升级版本比当前exe存放版本还要小，立即升级...
	if (lastVersion < LIBOBS_API_VER) {
		lastUpdate = 0;
		config_set_int(App()->GlobalConfig(), "General", "LastUpdateCheck", 0);
	}
	// 计算当前时间与上次升级之间的时间差...
	long long t = (long long)time(nullptr);
	long long secs = t - lastUpdate;
	// 时间差超过4天，开始检查并执行升级...
	if (secs > UPDATE_CHECK_INTERVAL) {
		this->CheckForUpdates(false);
	}
}

void CLoginMini::CheckForUpdates(bool manualUpdate)
{
	// 屏蔽升级操作菜单，避免在升级过程中重复升级...
	if (updateCheckThread && updateCheckThread->isRunning())
		return;
	// 创建升级线程，并启动之 => 可以手动直接升级...
	updateCheckThread = new AutoUpdateThread(manualUpdate);
	updateCheckThread->start();

	UNUSED_PARAMETER(manualUpdate);
}

void CLoginMini::updateCheckFinished()
{
}

CLoginMini::CLoginMini(QWidget *parent)
  : QDialog(parent)
  , m_nDBUserID(-1)
  , m_nDBRoomID(-1)
  , m_uTcpTimeID(0)
  , m_lpMovieGif(NULL)
  , m_lpLoadBack(NULL)
  , m_nOnLineTimer(-1)
  , m_nTcpSocketFD(-1)
  , m_nCenterTimer(-1)
  , m_nCenterTcpPort(0)
  , m_CenterSession(NULL)
  , m_bIsThirdUsed(false)
  , ui(new Ui::LoginMini)
  , m_eLoginCmd(kNoneCmd)
  , m_eMiniState(kCenterAddr)
{
	ui->setupUi(this);
	this->initWindow();
}

CLoginMini::~CLoginMini()
{
	if (m_nCenterTimer >= 0) {
		this->killTimer(m_nCenterTimer);
		m_nCenterTimer = -1;
	}
	if (m_nOnLineTimer >= 0) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
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
	// 窗口退出时，需要保证升级线程已经完全退出了...
	if (updateCheckThread && updateCheckThread->isRunning()) {
		updateCheckThread->wait();
	}
	// 这里需要显示删除升级线程对象...
	if (updateCheckThread) {
		delete updateCheckThread;
		updateCheckThread = NULL;
	}
	// 将发生变化的配置信息存入配置文件 => 主要存放global.ini文件...
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
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
	// 验证并判断是否需要使用第三方二维码模式...
	m_strThirdURI = App()->GetThirdURI();
	m_bIsThirdUsed = ((strnicmp(m_strThirdURI.c_str(), "http://", strlen("http://")) == 0) ? true : m_bIsThirdUsed);
	m_bIsThirdUsed = ((strnicmp(m_strThirdURI.c_str(), "https://", strlen("https://")) == 0) ? true : m_bIsThirdUsed);
	// 更新扫码状态栏...
	m_strScan.clear();
	ui->iconScan->hide();
	// FramelessWindowHint属性设置窗口去除边框;
	// WindowMinimizeButtonHint 属性设置在窗口最小化时，点击任务栏窗口可以显示出原窗口;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// 设置窗口背景透明 => 设置之后会造成全黑问题 => 顶层窗口需要用背景蒙板，普通窗口没问题...
	this->setAttribute(Qt::WA_TranslucentBackground);
	// 关闭窗口时释放资源;
	this->setAttribute(Qt::WA_DeleteOnClose);
	// 重新设定窗口大小...
	this->resize(360, 440);
	// 加载PTZ窗口的层叠显示样式表...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// 设置窗口图标 => 必须用png图片 => 解决有些机器不认ico，造成左上角图标无法显示...
	this->setWindowIcon(QIcon(":/res/images/student.png"));
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
	int nYPos = (rcRect.height() - rcSize.height()) / 2 - ui->btnClose->height() * 2 - 10;
	m_lpLoadBack->move(nXPos, nYPos);
	// 关联点击最小化按钮|关闭按钮的信号槽事件...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	// 关联网络信号槽反馈结果事件...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// 发起获取中心服务器的TCP地址和端口的命令...
	this->doWebGetCenterAddr();
	// 只进行一次更新状态检测...
	this->TimedCheckForUpdates();
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
	// 更新扫码状态栏...
	m_strScan.clear();
	ui->iconScan->hide();
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
	ui->titleScan->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
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
	// 如果是第三模式验证，访问不同的接口...
	if (m_strThirdURI.size() > 0 && m_bIsThirdUsed) {
		this->doWebGetMiniThirdCode();
		return;
	}
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
	// 二维码场景值增加随机数功能...
	//srand((unsigned int)time(NULL));
	// 准备需要的汇报数据 => POST数据包...
	Json::Value itemData;
	itemData["width"] = kQRCodeWidth;
	itemData["page"] = m_strMiniPath.c_str();
	// 二维码场景值 => 用户类型 + 套接字 + 时间标识 => 总长度不超过32字节...
	itemData["scene"] = QString("%1_%2_%3").arg(App()->GetClientType()).arg(m_nTcpSocketFD).arg(m_uTcpTimeID).toStdString();
	QString strRequestURL = QString("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%1").arg(m_strMiniToken.c_str());
	QString strContentVal = QString("%1").arg(itemData.toStyledString().c_str());
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

// 正式登录指定房间 => 获取节点服务器信息...
void CLoginMini::doWebGetMiniLoginRoom()
{
	// 显示动画，修改状态...
	ui->iconScan->hide();
	m_lpLoadBack->show();
	m_eMiniState = kMiniLoginRoom;
	m_strScan = QStringLiteral("正在登录已选择的房间...");
	ui->titleScan->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	// 构造网络访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strContentVal = QString("room_id=%1&type_id=%2&debug_mode=%3").arg(m_nDBRoomID).arg(App()->GetClientType()).arg(App()->IsDebugMode());
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Gather/loginLiveRoom");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

void CLoginMini::onProcMiniLoginRoom(QNetworkReply *reply)
{
	Json::Value value;
	bool bIsError = false;
	ASSERT(m_eMiniState == kMiniLoginRoom);
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
	do {
		// 解析json数据包失败，设置标志跳出...
		if (!this->parseJson(strData, value, false)) {
			m_strScan = m_strQRNotice;
			m_strQRNotice.clear();
			bIsError = true;
			break;
		}
		// 继续解析获取到的存储服务器地址和端口...
		string strTrackerAddr = CStudentApp::getJsonString(value["tracker_addr"]);
		string strTrackerPort = CStudentApp::getJsonString(value["tracker_port"]);
		// 继续解析获取到的远程中转服务器地址和端口...
		string strRemoteAddr = CStudentApp::getJsonString(value["remote_addr"]);
		string strRemotePort = CStudentApp::getJsonString(value["remote_port"]);
		// 继续解析获取到的udp服务器地址和端口...
		string strUdpAddr = CStudentApp::getJsonString(value["udp_addr"]);
		string strUdpPort = CStudentApp::getJsonString(value["udp_port"]);
		// 继续解析获取到的房间里的讲师和学生数量...
		string strTeacherCount = CStudentApp::getJsonString(value["teacher"]);
		string strStudentCount = CStudentApp::getJsonString(value["student"]);
		if (strTrackerAddr.size() <= 0 || strTrackerPort.size() <= 0 || strRemoteAddr.size() <= 0 || strRemotePort.size() <= 0 ||
			strUdpAddr.size() <= 0 || strUdpPort.size() <= 0 || strTeacherCount.size() <= 0 || strStudentCount.size() <= 0) {
			m_strScan = QTStr("Student.Room.Json");
			bIsError = true;
			break;
		}
		// 计算并判断房间里的讲师数量，大于0，不能登录...
		int nTeacherCount = atoi(strTeacherCount.c_str());
		int nStudentCount = atoi(strStudentCount.c_str());
		blog(LOG_INFO, "Teacher: %d, Student: %d", nTeacherCount, nStudentCount);
		// 获取到直播的分解数据，并将直播地址保存到 => obs-student/global.ini...
		config_set_int(App()->GlobalConfig(), "General", "LiveRoomID", m_nDBRoomID);
		// 将获取到的相关地址信息存放到全局对象当中...
		App()->SetTrackerAddr(strTrackerAddr);
		App()->SetTrackerPort(atoi(strTrackerPort.c_str()));
		App()->SetRemoteAddr(strRemoteAddr);
		App()->SetRemotePort(atoi(strRemotePort.c_str()));
		App()->SetUdpAddr(strUdpAddr);
		App()->SetUdpPort(atoi(strUdpPort.c_str()));
	} while (false);
	// 发生错误，关闭动画，显示图标|信息，文字左对齐...
	if (bIsError) {
		m_lpLoadBack->hide(); ui->iconScan->show();
		ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
		ui->iconScan->setStyleSheet(strStyle);
		this->update();
		return;
	}
	// 获取登录用户的详细信息...
	this->doWebGetMiniUserInfo();
}

// 修改状态，获取绑定登录用户的详细信息...
void CLoginMini::doWebGetMiniUserInfo()
{
	// 显示动画，修改状态...
	ui->iconScan->hide();
	m_lpLoadBack->show();
	m_eMiniState = kMiniUserInfo;
	m_strScan = QStringLiteral("正在获取已登录用户信息...");
	ui->titleScan->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	// 构造网络访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strContentVal = QString("user_id=%1&room_id=%2&type_id=%3").arg(m_nDBUserID).arg(m_nDBRoomID).arg(App()->GetClientType());
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Mini/getLoginUser");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

void CLoginMini::onProcMiniUserInfo(QNetworkReply *reply)
{
	Json::Value value;
	bool bIsError = false;
	ASSERT(m_eMiniState == kMiniUserInfo);
	QByteArray & theByteArray = reply->readAll();
	string & strData = theByteArray.toStdString();
	blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
	do {
		// 解析json数据包失败，设置标志跳出...
		if (!this->parseJson(strData, value, false)) {
			m_strScan = m_strQRNotice;
			m_strQRNotice.clear();
			bIsError = true;
			break;
		}
		// 判断是否获取到了正确的用户和房间信息...
		if (!value.isMember("user") || !value.isMember("room")) {
			m_strScan = QStringLiteral("错误提示：无法从服务器获取用户或房间详情。");
			bIsError = true;
			break;
		}
		// 判断获取到的用户信息是否有效...
		int nDBUserID = atoi(CStudentApp::getJsonString(value["user"]["user_id"]).c_str());
		int nUserType = atoi(CStudentApp::getJsonString(value["user"]["user_type"]).c_str());
		ASSERT(nDBUserID == m_nDBUserID);
		// 因为是学生端，无需检测用户身份类型...
		// 判断是否获取到了有效的流量统计记录字段...
		int nDBFlowID = (value.isMember("flow_id") ? atoi(CStudentApp::getJsonString(value["flow_id"]).c_str()) : 0);
		if (!value.isMember("flow_id") || nDBFlowID <= 0) {
			m_strScan = QStringLiteral("错误提示：无法从服务器获取流量统计记录编号。");
			bIsError = true;
			break;
		}
		// 保存流量记录到全局变量当中...
		App()->SetDBFlowID(nDBFlowID);
	} while (false);
	// 发生错误，关闭动画，显示图标|信息，文字左对齐...
	if (bIsError) {
		m_lpLoadBack->hide(); ui->iconScan->show();
		ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
		ui->iconScan->setStyleSheet(strStyle);
		this->update();
		return;
	}
	// 进行页面跳转，关闭小程序登录窗口...
	emit this->doTriggerMiniSuccess();
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
	case kMiniUserInfo: this->onProcMiniUserInfo(reply); break;
	case kMiniLoginRoom: this->onProcMiniLoginRoom(reply); break;
	case kMiniThirdCode: this->onProcMiniThirdCode(reply); break;
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
	m_strCenterTcpAddr = CStudentApp::getJsonString(value["udpcenter_addr"]);
	m_nCenterTcpPort = atoi(CStudentApp::getJsonString(value["udpcenter_port"]).c_str());
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
	this->connect(m_CenterSession, SIGNAL(doTriggerBindMini(int, int, int)), this, SLOT(onTriggerBindMini(int, int, int)));
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
	m_uTcpTimeID = m_CenterSession->GetTcpTimeID();
	// 每隔30秒检测一次，讲师端在中心服务器上在线汇报通知...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// 发起获取小程序Token值的网络命令...
	this->doWebGetMiniToken();
	
	/*== 仅供快速测试 ==*/
	//m_nDBUserID = 1;
	//m_nDBRoomID = 200005;
	// 一切正常，开始登录指定的房间...
	//this->doWebGetMiniLoginRoom();
}

// 响应中心会话反馈的小程序绑定登录信号槽事件通知...
void CLoginMini::onTriggerBindMini(int nUserID, int nBindCmd, int nRoomID)
{
	// 如果当前状态不是二维码显示状态，直接返回...
	//if (m_eMiniState != kMiniQRCode)
	//	return;
	// 根据绑定子命令显示不同的信息或图片状态...
	if (nBindCmd == kScanCmd) {
		m_strScan = QStringLiteral("已扫描成功，请在微信中输入教室密码，完成授权登录。");
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -46px;");
		ui->iconScan->setStyleSheet(strStyle);
		ui->iconScan->show();
		this->update();
	} else if (nBindCmd == kCancelCmd) {
		m_strScan = QStringLiteral("您已取消此次登录，您可再次扫描登录，或关闭窗口。");
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
		ui->iconScan->setStyleSheet(strStyle);
		ui->iconScan->show();
		this->update();
	} else if (nBindCmd == kSaveCmd) {
		// 如果用户编号|房间编号无效，显示错误信息...
		if (nUserID <= 0 || nRoomID <= 0) {
			m_strScan = QStringLiteral("用户编号或房间编号无效，请确认后重新用微信扫描登录。");
			QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
			ui->iconScan->setStyleSheet(strStyle);
			ui->iconScan->show();
			this->update();
			return;
		}
		// 保存用户编号|房间编号...
		m_nDBUserID = nUserID;
		m_nDBRoomID = nRoomID;
		// 一切正常，开始登录指定的房间...
		this->doWebGetMiniLoginRoom();
	}
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

// 获取第三方二维码...
void CLoginMini::doWebGetMiniThirdCode()
{
	// 必须是第三方模式并且第三方链接地址有效...
	ASSERT(m_bIsThirdUsed && m_strThirdURI.size() > 0);
	// 修改获取第三方二维码状态...
	m_eMiniState = kMiniThirdCode;
	m_strQRNotice = QStringLiteral("正在获取第三方凭证...");
	// 构造凭证访问地址，发起网络请求...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	// 二维码场景值 => 用户类型 + 套接字 + 时间标识 => 总长度不超过32字节...
	QString strRequestURL = QString("%1").arg(m_strThirdURI.c_str());
	QString strContentVal = QString("scene=%1_%2_%3").arg(App()->GetClientType()).arg(m_nTcpSocketFD).arg(m_uTcpTimeID);
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// 更新显示界面内容...
	this->update();
}

// 处理第三方反馈的二维码图片内容...
void CLoginMini::onProcMiniThirdCode(QNetworkReply *reply)
{
	// 无论对错，都要关闭加载动画...
	m_lpLoadBack->hide();
	// 读取获取到的网络数据内容...
	ASSERT(m_eMiniState == kMiniThirdCode);
	QByteArray & theByteArray = reply->readAll();
	// 从网路数据直接构造二维码的图片信息|设定默认扫码信息...
	if (m_QPixQRCode.loadFromData(theByteArray)) {
		m_strScan = QStringLiteral("请使用微信扫描二维码登录");
		m_strQRNotice.clear();
		this->update();
		return;
	}
	// 解除位图显示对象...
	Json::Value value;
	m_QPixQRCode.detach();
	// 构造二维码图片对象失败，进行错误解析...
	string & strData = theByteArray.toStdString();
	if (!this->parseJson(strData, value, false)) {
		this->update();
		return;
	}
	// 解析json数据包仍然失败，显示特定的错误信息...
	m_strQRNotice = QStringLiteral("解析获取到的JSON数据失败");
	this->update();
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
	m_strMiniToken = CStudentApp::getJsonString(value["access_token"]);
	m_strMiniPath = CStudentApp::getJsonString(value["mini_path"]);
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
		m_strScan = QStringLiteral("请使用微信扫描二维码登录");
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
		string & strMsg = CStudentApp::getJsonString(outValue[lpszMsg]);
		string & strCode = CStudentApp::getJsonString(outValue[lpszCode]);
		m_strQRNotice = QString("%1").arg(strMsg.c_str());
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
