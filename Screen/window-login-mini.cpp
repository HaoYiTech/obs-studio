
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
	// ����Ƿ����Զ����¿��أ�Ĭ���Ǵ��ڿ���״̬ => CStudentApp::InitGlobalConfigDefaults()...
	if (!config_get_bool(App()->GlobalConfig(), "General", "EnableAutoUpdates"))
		return;
	// ע�⣺LastUpdateCheckֻ���û�������Ժ����ѡ��Ż�������(4���)���������ͬ�汾��Ҳ��ÿ�����������⣻
	long long lastUpdate = config_get_int(App()->GlobalConfig(), "General", "LastUpdateCheck");
	uint32_t lastVersion = config_get_int(App()->GlobalConfig(), "General", "LastVersion");
	// ����ϴ������汾�ȵ�ǰexe��Ű汾��ҪС����������...
	if (lastVersion < LIBOBS_API_VER) {
		lastUpdate = 0;
		config_set_int(App()->GlobalConfig(), "General", "LastUpdateCheck", 0);
	}
	// ���㵱ǰʱ�����ϴ�����֮���ʱ���...
	long long t = (long long)time(nullptr);
	long long secs = t - lastUpdate;
	// ʱ����4�죬��ʼ��鲢ִ������...
	if (secs > UPDATE_CHECK_INTERVAL) {
		this->CheckForUpdates(false);
	}
}

void CLoginMini::CheckForUpdates(bool manualUpdate)
{
	// �������������˵��������������������ظ�����...
	/*if (updateCheckThread && updateCheckThread->isRunning())
		return;
	// ���������̣߳�������֮ => �����ֶ�ֱ������...
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
	// �������仯��������Ϣ���������ļ� => ��Ҫ���global.ini�ļ�...
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
}

void CLoginMini::closeEvent(QCloseEvent *event)
{
	// �����ڵ����걣������...
	/*if (this->isVisible()) {
		config_set_string(App()->GlobalConfig(),
			"BasicWindow", "geometry",
			saveGeometry().toBase64().constData());
	}*/
	// ִ��close�¼�...
	QWidget::closeEvent(event);
	if (!event->isAccepted())
		return;
	// ��ӡ�ر���־...
	blog(LOG_INFO, SHUTDOWN_SEPARATOR);
	// �ȴ��Զ���������̵߳��˳�...
	//if (updateCheckThread != NULL) {
	//	updateCheckThread->wait();
	//}
	// �����˳��¼�֪ͨ...
	//App()->doLogoutEvent();
	// ���ùر��˳��ӿ�...
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

// ����Բ�Ǵ��ڱ��� => �����Ƕ��㴰�ڣ���Ҫʹ���ɰ�...
void CLoginMini::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QPainterPath pathBack;

	// ��������������ɫ => Բ��ģʽ => WA_TranslucentBackground
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	pathBack.addRoundedRect(QRect(0, 0, this->width(), this->height()), 4, 4);
	painter.fillPath(pathBack, QBrush(QColor(0, 122, 204)));

	// ��ʾ�汾|ɨ��|����...
	ui->titleVer->setText(m_strVer);
	//ui->titleScan->setText(m_strScan);
	//ui->titleQR->setText(m_strQRNotice);

	QWidget::paintEvent(event);
}

void CLoginMini::initWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);
	// FramelessWindowHint�������ô���ȥ���߿�;
	// WindowMinimizeButtonHint ���������ڴ�����С��ʱ��������������ڿ�����ʾ��ԭ����;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// ���ô��ڱ���͸�� => ����֮������ȫ������ => ���㴰����Ҫ�ñ����ɰ壬��ͨ����û����...
	this->setAttribute(Qt::WA_TranslucentBackground);
	// �رմ���ʱ�ͷ���Դ;
	this->setAttribute(Qt::WA_DeleteOnClose);
	// �����趨���ڴ�С...
	this->resize(360, 440);
	// ����PTZ���ڵĲ����ʾ��ʽ��...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// ���ô���ͼ��...
	this->setWindowIcon(QIcon(":/res/images/screen.png"));
	// ��ʼ���汾������Ϣ...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// ���ñ�������ͼƬ => gif
	//m_lpMovieGif = new QMovie();
	//m_lpLoadBack = new QLabel(this);
	//m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	//m_lpLoadBack->setMovie(m_lpMovieGif);
	//m_lpMovieGif->start();
	//m_lpLoadBack->raise();
	// �������ƶ������ھ�����ʾ...
	//QRect rcRect = this->rect();
	//QRect rcSize = m_lpMovieGif->frameRect();
	//int nXPos = (rcRect.width() - rcSize.width()) / 2;
	//int nYPos = (rcRect.height() - rcSize.height()) / 2 - ui->btnClose->height() * 2 - 10;
	//m_lpLoadBack->move(nXPos, nYPos);
	// ���������С����ť|�رհ�ť���źŲ��¼�...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	// ���������źŲ۷�������¼�...
	//connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// �����ȡ���ķ�������TCP��ַ�Ͷ˿ڵ�����...
	//this->doWebGetCenterAddr();
	// ֻ����һ�θ���״̬���...
	//this->TimedCheckForUpdates();

	// �������½�������ͼ��...
	this->SystemTray(true);

	// �ָ���������λ��...
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
		m_strQRNotice = QStringLiteral("������ȡ����JSON����ʧ��");
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

// ����ͨ�� mousePressEvent | mouseMoveEvent | mouseReleaseEvent �����¼�ʵ��������϶��������ƶ����ڵ�Ч��;
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
