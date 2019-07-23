
#include "window-login-mini.h"
#include "screen-app.h"
#include <QDesktopWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QBitmap>
#include <QMovie>
#include <QTimer>
#include <time.h>

#define STARTUP_SEPARATOR   "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR 	"==== Shutting down =================================================="

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
	/*if (updateCheckThread && updateCheckThread->isRunning())
		return;
	// 创建升级线程，并启动之 => 可以手动直接升级...
	updateCheckThread = new AutoUpdateThread(manualUpdate);
	updateCheckThread->start();*/
}

CLoginMini::CLoginMini(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::LoginMini)
{
	ui->setupUi(this);
	this->initWindow();
}

CLoginMini::~CLoginMini()
{
	// 将发生变化的配置信息存入配置文件 => 主要存放global.ini文件...
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
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
	// 打印关闭日志...
	blog(LOG_INFO, SHUTDOWN_SEPARATOR);
	// 等待自动检查升级线程的退出...
	//if (updateCheckThread != NULL) {
	//	updateCheckThread->wait();
	//}
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

	// 显示版本|扫码|警告...
	ui->titleVer->setText(m_strVer);
	//ui->titleScan->setText(m_strScan);
	//ui->titleQR->setText(m_strQRNotice);

	QWidget::paintEvent(event);
}

void CLoginMini::initWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);
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
	// 设置窗口图标...
	this->setWindowIcon(QIcon(":/res/images/screen.png"));
	// 初始化版本文字信息...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// 设置背景动画图片 => gif
	//m_lpMovieGif = new QMovie();
	//m_lpLoadBack = new QLabel(this);
	//m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	//m_lpLoadBack->setMovie(m_lpMovieGif);
	//m_lpMovieGif->start();
	//m_lpLoadBack->raise();
	// 将动画移动到窗口居中显示...
	//QRect rcRect = this->rect();
	//QRect rcSize = m_lpMovieGif->frameRect();
	//int nXPos = (rcRect.width() - rcSize.width()) / 2;
	//int nYPos = (rcRect.height() - rcSize.height()) / 2 - ui->btnClose->height() * 2 - 10;
	//m_lpLoadBack->move(nXPos, nYPos);
	// 关联点击最小化按钮|关闭按钮的信号槽事件...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	// 关联网络信号槽反馈结果事件...
	//connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// 发起获取中心服务器的TCP地址和端口的命令...
	//this->doWebGetCenterAddr();
	// 只进行一次更新状态检测...
	//this->TimedCheckForUpdates();

	// 开启右下角任务栏图标...
	this->SystemTray(true);

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
	/*int nTimerID = inEvent->timerId();
	if (nTimerID == m_nCenterTimer) {
		this->doCheckSession();
	} else if (nTimerID == m_nOnLineTimer) {
		this->doCheckOnLine();
	}*/
}

bool CLoginMini::parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin)
{
	/*Json::Reader reader;
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
	}*/
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
