
#include "window-login-mini.h"
#include "qt-wrappers.hpp"
#include "FastSession.h"
#include "screen-app.h"
#include <QDesktopWidget>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMouseEvent>
#include <QLineEdit>
#include <QPainter>
#include <QBitmap>
#include <QMovie>
#include <QTimer>
#include <time.h>

#define UPDATE_CHECK_INTERVAL (60*60*24*4) /* 4 days */

void CLoginMini::TimedCheckForUpdates()
{
	// 检测是否开启自动更新开关，默认是处于开启状态 => CScreenApp::InitGlobalConfigDefaults()...
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
	/*if (updateCheckThread && updateCheckThread->isRunning())
		return;
	// 创建升级线程，并启动之 => 可以手动直接升级...
	updateCheckThread = new AutoUpdateThread(manualUpdate);
	updateCheckThread->start();*/
}

CLoginMini::CLoginMini(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::LoginMini)
  , m_RemoteSession(NULL)
  , m_eMiniState(kMiniLoginRoom)
{
	ui->setupUi(this);
	this->initWindow();
}

CLoginMini::~CLoginMini()
{
	this->killTimer(m_nOnLineTimer);
	if (m_lpMovieGif != NULL) {
		delete m_lpMovieGif;
		m_lpMovieGif = NULL;
	}
	if (m_lpLoadBack != NULL) {
		delete m_lpLoadBack;
		m_lpLoadBack = NULL;
	}
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 窗口退出时，需要保证升级线程已经完全退出了...
	/*if (updateCheckThread && updateCheckThread->isRunning()) {
		updateCheckThread->wait();
	}
	// 这里需要显示删除升级线程对象...
	if (updateCheckThread) {
		delete updateCheckThread;
		updateCheckThread = NULL;
	}*/
	// 将发生变化的配置信息存入配置文件 => 主要存放global.ini文件...
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
}

// 重载但不执行，避免Esc键...
void CLoginMini::reject()
{
}

void CLoginMini::closeEvent(QCloseEvent *event)
{
	// 将窗口的坐标保存起来...
	/*if (this->isVisible()) {
		config_set_string(App()->GlobalConfig(),
			"BasicWindow", "geometry",
			saveGeometry().toBase64().constData());
	}*/
	// 执行close事件...
	QWidget::closeEvent(event);
	if (!event->isAccepted())
		return;
	// 等待自动检查升级线程的退出...
	//if (updateCheckThread != NULL) {
	//	updateCheckThread->wait();
	//}
	
	App()->ClearSceneData();

	// 调用退出事件通知...
	//App()->doLogoutEvent();
	// 调用关闭退出接口...
	//App()->quit();
}

void CLoginMini::loadStyleSheet(const QString &sheetName)
{
	QFile file(sheetName);
	file.open(QFile::ReadOnly);
	if (file.isOpen()) {
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

	// 显示版本|警告...
	ui->titleVer->setText(m_strVer);
	ui->titleScan->setText(m_strScan);

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
	this->resize(360, 440);
	// 加载窗口的层叠显示样式表...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// 设置窗口图标...
	this->setWindowIcon(QIcon(":/res/images/screen.png"));
	// 更新扫码状态栏...
	m_strScan.clear();
	ui->iconScan->hide();
	// 暗注释，设置学生姓名...
	ui->loginUser->setPlaceholderText(QTStr("MINI.Window.UserHolderText"));
	ui->loginUser->setMaxLength(10);
	// 暗注释，并限定输入数字的范围...
	ui->loginRoom->setPlaceholderText(QTStr("MINI.Window.RoomHolderText"));
	ui->loginRoom->setValidator(new QIntValidator(0, 999999999, this));
	ui->loginRoom->setMaxLength(10);
	// 暗注释，并限定输入数字的范围...
	ui->loginPass->setPlaceholderText(QTStr("MINI.Window.PassHolderText"));
	ui->loginPass->setValidator(new QIntValidator(0, 999999, this));
	ui->loginPass->setMaxLength(6);
	// 初始化版本文字信息...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// 设置背景动画图片 => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	m_lpLoadBack->hide();
	ui->loginButton->show();
	ui->logoutButton->hide();
	// 将动画移动到窗口居中显示...
	QRect rcRect = this->rect();
	QRect rcSize = m_lpMovieGif->frameRect();
	int nXPos = (rcRect.width() - rcSize.width()) / 2;
	int nYPos = ui->btnClose->height() * 2 + 10;
	m_lpLoadBack->move(nXPos, nYPos);
	// 关联点击最小化按钮|关闭按钮的信号槽事件...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->loginButton, SIGNAL(clicked()), this, SLOT(onClickLoginButton()));
	connect(ui->logoutButton, SIGNAL(clicked()), this, SLOT(onClickLogoutButton()));
	// 编辑窗口的回车事件也跟登录按钮的点击事件绑定上 => 这里不用绑定，已经与default相关...
	//connect(ui->loginUser, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	//connect(ui->loginRoom, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	//connect(ui->loginPass, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	// 关联网络信号槽反馈结果事件...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// 只进行一次更新状态检测...
	//this->TimedCheckForUpdates();

	// 开启右下角任务栏图标...
	this->SystemTray(true);

	// 读取已输入信息更新到界面上...
	const char * lpUser = config_get_string(App()->GlobalConfig(), "General", "User");
	const char * lpRoom = config_get_string(App()->GlobalConfig(), "General", "Room");
	const char * lpPass = config_get_string(App()->GlobalConfig(), "General", "Pass");
	if (lpUser != nullptr) { m_strUser = QString("%1").arg(lpUser); }
	if (lpRoom != nullptr) { m_strRoom = QString("%1").arg(lpRoom); }
	if (lpPass != nullptr) { m_strPass = QString("%1").arg(lpPass); }
	ui->loginUser->setText(m_strUser);
	ui->loginRoom->setText(m_strRoom);
	ui->loginPass->setText(m_strPass);

	// 恢复窗口坐标位置...
	/*const char *geometry = config_get_string(App()->GlobalConfig(), "BasicWindow", "geometry");
	if (geometry != NULL) {
		QByteArray byteArray = QByteArray::fromBase64(QByteArray(geometry));
		this->restoreGeometry(byteArray);
		QRect windowGeometry = this->normalGeometry();
		if (!WindowPositionValid(windowGeometry)) {
			const QRect & rect = App()->desktop()->geometry();
			setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), rect));
		}
	}*/
}

void CLoginMini::onClickLogoutButton()
{
	// 释放远程连接对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 删除中心在线心跳包检测对象...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// 初始化界面状态...
	m_eMiniState = kMiniLoginRoom;
	ui->loginButton->show();
	ui->logoutButton->hide();
	ui->loginUser->setDisabled(false);
	ui->loginRoom->setDisabled(false);
	ui->loginPass->setDisabled(false);
	m_lpLoadBack->hide();
	ui->iconScan->hide();
	m_strScan.clear();
	this->update();
}

// 响应点击登录按钮...
void CLoginMini::onClickLoginButton()
{
	// 释放远程连接对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 删除中心在线心跳包检测对象...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// 获取界面数据...
	m_strScan.clear();
	m_strUser = ui->loginUser->text();
	m_strRoom = ui->loginRoom->text();
	m_strPass = ui->loginPass->text();
	do {
		if (m_strUser.size() <= 0) {
			m_strScan = QTStr("Screen.User.None");
			ui->loginUser->setFocus();
			break;
		}
		if (m_strRoom.size() <= 0) {
			m_strScan = QTStr("Screen.Room.None");
			ui->loginRoom->setFocus();
			break;
		}
		if (m_strPass.size() <= 0) {
			m_strScan = QTStr("Screen.Pass.None");
			ui->loginPass->setFocus();
			break;
		}
		// 设置按钮状态...
		ui->loginButton->hide();
		ui->logoutButton->show();
		ui->loginUser->setDisabled(true);
		ui->loginRoom->setDisabled(true);
		ui->loginPass->setDisabled(true);
	} while (false);
	if (m_strScan.size() > 0) {
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
		ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		ui->iconScan->setStyleSheet(strStyle);
		ui->iconScan->show();
		this->update();
		return;
	}
	// 更新界面...
	this->update();
	// 一切正常，开始登录指定的房间...
	this->doWebGetMiniLoginRoom();
}

void CLoginMini::onReplyFinished(QNetworkReply *reply)
{
	// 如果发生网络错误，打印错误信息，跳出循环...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		m_strScan = QString("%1, %2").arg(reply->error()).arg(reply->errorString());
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// 读取完整的网络请求返回的内容数据包...
	int nStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	blog(LOG_INFO, "QT Status Code => %d", nStatusCode);
	// 根据状态分发处理过程...
	switch (m_eMiniState) {
	case kMiniLoginRoom: this->onProcMiniLoginRoom(reply); break;
	}
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
	QString strContentVal = QString("room_id=%1&type_id=%2&debug_mode=%3&room_pass=%4")
		.arg(m_strRoom).arg(App()->GetClientType()).arg(App()->IsDebugMode()).arg(m_strPass);
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
			bIsError = true;
			break;
		}
		// 继续解析获取到的存储服务器地址和端口...
		string strTrackerAddr = CScreenApp::getJsonString(value["tracker_addr"]);
		string strTrackerPort = CScreenApp::getJsonString(value["tracker_port"]);
		// 继续解析获取到的远程中转服务器地址和端口...
		string strRemoteAddr = CScreenApp::getJsonString(value["remote_addr"]);
		string strRemotePort = CScreenApp::getJsonString(value["remote_port"]);
		// 继续解析获取到的udp服务器地址和端口...
		string strUdpAddr = CScreenApp::getJsonString(value["udp_addr"]);
		string strUdpPort = CScreenApp::getJsonString(value["udp_port"]);
		// 继续解析获取到的房间里的讲师和学生数量...
		string strTeacherCount = CScreenApp::getJsonString(value["teacher"]);
		string strStudentCount = CScreenApp::getJsonString(value["student"]);
		if (strTrackerAddr.size() <= 0 || strTrackerPort.size() <= 0 || strRemoteAddr.size() <= 0 || strRemotePort.size() <= 0 ||
			strUdpAddr.size() <= 0 || strUdpPort.size() <= 0 || strTeacherCount.size() <= 0 || strStudentCount.size() <= 0) {
			m_strScan = QTStr("Screen.Room.Json");
			bIsError = true;
			break;
		}
		// 计算并判断房间里的讲师数量，大于0，不能登录...
		int nTeacherCount = atoi(strTeacherCount.c_str());
		int nStudentCount = atoi(strStudentCount.c_str());
		blog(LOG_INFO, "Teacher: %d, Student: %d", nTeacherCount, nStudentCount);

		// 获取到直播的分解数据，并将直播地址保存到 => obs-screen/global.ini...
		config_set_string(GetGlobalConfig(), "General", "User", m_strUser.toStdString().c_str());
		config_set_string(GetGlobalConfig(), "General", "Room", m_strRoom.toStdString().c_str());
		config_set_string(GetGlobalConfig(), "General", "Pass", m_strPass.toStdString().c_str());
		// 将发生变化的配置信息存入配置文件 => 主要存放global.ini文件...
		config_save_safe(GetGlobalConfig(), "tmp", nullptr);
		App()->SetRoomIDStr(m_strRoom.toStdString());
		App()->SetUserNameStr(m_strUser.toStdString());
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
		ui->loginButton->show();
		ui->logoutButton->hide();
		ui->loginUser->setDisabled(false);
		ui->loginRoom->setDisabled(false);
		ui->loginPass->setDisabled(false);
		m_lpLoadBack->hide(); ui->iconScan->show();
		ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
		ui->iconScan->setStyleSheet(strStyle);
		this->update();
		return;
	}
	// 发起连接中转服务器的会话对象...
	this->doTcpConnRemote();
}

void CLoginMini::doTcpConnRemote()
{
	// 修改连接中心服务器状态...
	m_eMiniState = kMiniRemoteConn;
	m_strScan = QStringLiteral("正在连接远程服务器...");
	// 创建中心会话对象 => 接收小程序反馈...
	ASSERT(m_RemoteSession == NULL);
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(App()->GetRemoteAddr().c_str(), App()->GetRemotePort());
	// 注册相关的事件反馈函数 => 通过信号槽消息绑定 => 采用异步投递模式...
	this->connect(m_RemoteSession, SIGNAL(doTriggerConnected()), this, SLOT(onTriggerConnected()), Qt::QueuedConnection);
	// 更新显示界面内容...
	this->update();
}

// 响应中心会话对象的事件通知信号槽...
void CLoginMini::onTriggerConnected()
{
	// 远程连接已退出，直接返回...
	if (m_RemoteSession == NULL)
		return;
	// 这里为了达到异步效果，使用了时钟...
	if (!m_RemoteSession->IsConnected()) {
		m_nFailedTimer = this->startTimer(10);
		return;
	}
	// 每隔30秒检测一次，讲师端在中心服务器上在线汇报通知...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// 更新提示界面信息...
	m_strScan = QStringLiteral("正在初始化屏幕抓取模块...");
	this->update();
	// 激发登录成功之后的事件通知 => 两次异步之后，避开了底层线程...
	emit this->doTriggerMiniSuccess();
}

/*void CLoginMini::doLoginSuccess()
{
	// 关闭启动时钟...
	this->killTimer(m_nSuccessTimer);
	m_nSuccessTimer = -1;
	// 更新提示界面信息...
	m_strScan = QStringLiteral("正在初始化屏幕抓取模块...");
	this->update();
	// 激发登录成功之后的事件通知 => 启动OBS...
	emit this->doTriggerMiniSuccess();
}*/

void CLoginMini::doLoginFailed()
{
	// 关闭启动时钟...
	this->killTimer(m_nFailedTimer);
	m_nFailedTimer = -1;
	// 更新界面显示元素...
	m_strScan = QString("%1%2").arg(QStringLiteral("连接远程服务器失败，错误号：")).arg(m_RemoteSession->GetErrCode());
	blog(LOG_INFO, "QT error => %d", m_RemoteSession->GetErrCode());
	m_eMiniState = kMiniLoginRoom;
	ui->loginButton->show();
	ui->logoutButton->hide();
	ui->loginUser->setDisabled(false);
	ui->loginRoom->setDisabled(false);
	ui->loginPass->setDisabled(false);
	m_lpLoadBack->hide(); ui->iconScan->show();
	ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -87px;");
	ui->iconScan->setStyleSheet(strStyle);
	// 删除远程连接对象...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// 更新界面...
	this->update();
}

void CLoginMini::doSuccessOBS()
{
	// OBS初始化成功...
	QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -46px;");
	ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_strScan = QStringLiteral("已经成功登录云教室系统...");
	ui->iconScan->setStyleSheet(strStyle);
	m_lpLoadBack->hide();
	ui->iconScan->show();
	this->update();
}

bool CLoginMini::doSendScreenSnap(uint8_t * inData, int nSize)
{
	if (m_RemoteSession == NULL || inData == NULL || nSize <= 0)
		return false;
	return m_RemoteSession->doSendScreenSnap((const char*)inData, nSize);
}

void CLoginMini::SystemTray(bool firstStarted)
{
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return;
	if (firstStarted) {
		this->SystemTrayInit();
	}
	if (trayIcon != nullptr) {
		trayIcon->show();
	}
	if (this->isVisible()) {
		showHide->setText(QTStr("Basic.SystemTray.Hide"));
	} else {
		showHide->setText(QTStr("Basic.SystemTray.Show"));
	}
}

void CLoginMini::SystemTrayInit()
{
	trayIcon = new QSystemTrayIcon(QIcon(":/res/images/screen.png"), this);
	trayIcon->setToolTip(QTStr("Basic.SystemTray.Name"));

	showHide = new QAction(QTStr("Basic.SystemTray.Show"), trayIcon);
	exit = new QAction(QTStr("Exit"), trayIcon);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(IconActivated(QSystemTrayIcon::ActivationReason)));
	connect(showHide, SIGNAL(triggered()), this, SLOT(ToggleShowHide()));
	connect(exit, SIGNAL(triggered()), this, SLOT(close()));

	trayMenu = new QMenu;
}

void CLoginMini::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger) {
		this->ToggleShowHide();
	} else if (reason == QSystemTrayIcon::Context) {
		trayMenu->clear();
		trayMenu->addAction(showHide);
		trayMenu->addAction(exit);
		trayMenu->popup(QCursor::pos());
	}
}

void CLoginMini::ToggleShowHide()
{
	bool showing = this->isVisible();
	if (showing) {
		/* check for modal dialogs */
		this->EnumDialogs();
		if (!modalDialogs.isEmpty() || !visMsgBoxes.isEmpty())
			return;
	}
	this->SetShowing(!showing);
}

void CLoginMini::SetShowing(bool showing)
{
	if (!showing && this->isVisible()) {
		config_set_string(App()->GlobalConfig(),
			"BasicWindow", "geometry",
			saveGeometry().toBase64().constData());

		/* hide all visible child dialogs */
		visDlgPositions.clear();
		if (!visDialogs.isEmpty()) {
			for (QDialog *dlg : visDialogs) {
				visDlgPositions.append(dlg->pos());
				dlg->hide();
			}
		}

		if (showHide) {
			showHide->setText(QTStr("Basic.SystemTray.Show"));
		}
		QTimer::singleShot(250, this, SLOT(hide()));
		this->setVisible(false);
	} else if (showing && !this->isVisible()) {
		if (showHide) {
			showHide->setText(QTStr("Basic.SystemTray.Hide"));
		}
		QTimer::singleShot(250, this, SLOT(show()));
		this->setVisible(true);

		/* show all child dialogs that was visible earlier */
		if (!visDialogs.isEmpty()) {
			for (int i = 0; i < visDialogs.size(); ++i) {
				QDialog *dlg = visDialogs[i];
				dlg->move(visDlgPositions[i]);
				dlg->show();
			}
		}
	}
}

void CLoginMini::EnumDialogs()
{
	visDialogs.clear();
	modalDialogs.clear();
	visMsgBoxes.clear();

	/* fill list of Visible dialogs and Modal dialogs */
	QList<QDialog*> dialogs = findChildren<QDialog*>();
	for (QDialog *dialog : dialogs) {
		if (dialog->isVisible()) {
			visDialogs.append(dialog);
		}
		if (dialog->isModal()) {
			modalDialogs.append(dialog);
		}
	}

	/* fill list of Visible message boxes */
	QList<QMessageBox*> msgBoxes = findChildren<QMessageBox*>();
	for (QMessageBox *msgbox : msgBoxes) {
		if (msgbox->isVisible()) {
			visMsgBoxes.append(msgbox);
		}
	}
}

void CLoginMini::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nOnLineTimer) {
		this->doCheckOnLine();
	} else if (nTimerID == m_nFailedTimer) {
		this->doLoginFailed();
	}
}

bool CLoginMini::doCheckOnLine()
{
	if (m_RemoteSession == NULL)
		return false;
	return m_RemoteSession->doSendOnLineCmd();
}

bool CLoginMini::parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin)
{
	Json::Reader reader;
	if (!reader.parse(inData, outValue)) {
		m_strScan = QStringLiteral("解析获取到的JSON数据失败");
		return false;
	}
	const char * lpszCode = bIsWeiXin ? "errcode" : "err_code";
	const char * lpszMsg = bIsWeiXin ? "errmsg" : "err_msg";
	if (outValue[lpszCode].asBool()) {
		string & strMsg = CScreenApp::getJsonString(outValue[lpszMsg]);
		string & strCode = CScreenApp::getJsonString(outValue[lpszCode]);
		m_strScan = QString("%1").arg(strMsg.c_str());
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
