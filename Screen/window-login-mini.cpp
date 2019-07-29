
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
	// ����Ƿ����Զ����¿��أ�Ĭ���Ǵ��ڿ���״̬ => CScreenApp::InitGlobalConfigDefaults()...
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
	// �����˳�ʱ����Ҫ��֤�����߳��Ѿ���ȫ�˳���...
	/*if (updateCheckThread && updateCheckThread->isRunning()) {
		updateCheckThread->wait();
	}
	// ������Ҫ��ʾɾ�������̶߳���...
	if (updateCheckThread) {
		delete updateCheckThread;
		updateCheckThread = NULL;
	}*/
	// �������仯��������Ϣ���������ļ� => ��Ҫ���global.ini�ļ�...
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
}

// ���ص���ִ�У�����Esc��...
void CLoginMini::reject()
{
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
	// �ȴ��Զ���������̵߳��˳�...
	//if (updateCheckThread != NULL) {
	//	updateCheckThread->wait();
	//}
	
	App()->ClearSceneData();

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

	// ��ʾ�汾|����...
	ui->titleVer->setText(m_strVer);
	ui->titleScan->setText(m_strScan);

	QWidget::paintEvent(event);
}

void CLoginMini::initWindow()
{
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
	// ���ش��ڵĲ����ʾ��ʽ��...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// ���ô���ͼ��...
	this->setWindowIcon(QIcon(":/res/images/screen.png"));
	// ����ɨ��״̬��...
	m_strScan.clear();
	ui->iconScan->hide();
	// ��ע�ͣ�����ѧ������...
	ui->loginUser->setPlaceholderText(QTStr("MINI.Window.UserHolderText"));
	ui->loginUser->setMaxLength(10);
	// ��ע�ͣ����޶��������ֵķ�Χ...
	ui->loginRoom->setPlaceholderText(QTStr("MINI.Window.RoomHolderText"));
	ui->loginRoom->setValidator(new QIntValidator(0, 999999999, this));
	ui->loginRoom->setMaxLength(10);
	// ��ע�ͣ����޶��������ֵķ�Χ...
	ui->loginPass->setPlaceholderText(QTStr("MINI.Window.PassHolderText"));
	ui->loginPass->setValidator(new QIntValidator(0, 999999, this));
	ui->loginPass->setMaxLength(6);
	// ��ʼ���汾������Ϣ...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// ���ñ�������ͼƬ => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	m_lpLoadBack->hide();
	ui->loginButton->show();
	ui->logoutButton->hide();
	// �������ƶ������ھ�����ʾ...
	QRect rcRect = this->rect();
	QRect rcSize = m_lpMovieGif->frameRect();
	int nXPos = (rcRect.width() - rcSize.width()) / 2;
	int nYPos = ui->btnClose->height() * 2 + 10;
	m_lpLoadBack->move(nXPos, nYPos);
	// ���������С����ť|�رհ�ť���źŲ��¼�...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->loginButton, SIGNAL(clicked()), this, SLOT(onClickLoginButton()));
	connect(ui->logoutButton, SIGNAL(clicked()), this, SLOT(onClickLogoutButton()));
	// �༭���ڵĻس��¼�Ҳ����¼��ť�ĵ���¼����� => ���ﲻ�ð󶨣��Ѿ���default���...
	//connect(ui->loginUser, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	//connect(ui->loginRoom, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	//connect(ui->loginPass, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
	// ���������źŲ۷�������¼�...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// ֻ����һ�θ���״̬���...
	//this->TimedCheckForUpdates();

	// �������½�������ͼ��...
	this->SystemTray(true);

	// ��ȡ��������Ϣ���µ�������...
	const char * lpUser = config_get_string(App()->GlobalConfig(), "General", "User");
	const char * lpRoom = config_get_string(App()->GlobalConfig(), "General", "Room");
	const char * lpPass = config_get_string(App()->GlobalConfig(), "General", "Pass");
	if (lpUser != nullptr) { m_strUser = QString("%1").arg(lpUser); }
	if (lpRoom != nullptr) { m_strRoom = QString("%1").arg(lpRoom); }
	if (lpPass != nullptr) { m_strPass = QString("%1").arg(lpPass); }
	ui->loginUser->setText(m_strUser);
	ui->loginRoom->setText(m_strRoom);
	ui->loginPass->setText(m_strPass);

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

void CLoginMini::onClickLogoutButton()
{
	// �ͷ�Զ�����Ӷ���...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// ɾ����������������������...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// ��ʼ������״̬...
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

// ��Ӧ�����¼��ť...
void CLoginMini::onClickLoginButton()
{
	// �ͷ�Զ�����Ӷ���...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// ɾ����������������������...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// ��ȡ��������...
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
		// ���ð�ť״̬...
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
	// ���½���...
	this->update();
	// һ����������ʼ��¼ָ���ķ���...
	this->doWebGetMiniLoginRoom();
}

void CLoginMini::onReplyFinished(QNetworkReply *reply)
{
	// �������������󣬴�ӡ������Ϣ������ѭ��...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		m_strScan = QString("%1, %2").arg(reply->error()).arg(reply->errorString());
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// ��ȡ�������������󷵻ص��������ݰ�...
	int nStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	blog(LOG_INFO, "QT Status Code => %d", nStatusCode);
	// ����״̬�ַ��������...
	switch (m_eMiniState) {
	case kMiniLoginRoom: this->onProcMiniLoginRoom(reply); break;
	}
}

// ��ʽ��¼ָ������ => ��ȡ�ڵ��������Ϣ...
void CLoginMini::doWebGetMiniLoginRoom()
{
	// ��ʾ�������޸�״̬...
	ui->iconScan->hide();
	m_lpLoadBack->show();
	m_eMiniState = kMiniLoginRoom;
	m_strScan = QStringLiteral("���ڵ�¼��ѡ��ķ���...");
	ui->titleScan->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	// ����������ʵ�ַ��������������...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strContentVal = QString("room_id=%1&type_id=%2&debug_mode=%3&room_pass=%4")
		.arg(m_strRoom).arg(App()->GetClientType()).arg(App()->IsDebugMode()).arg(m_strPass);
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Gather/loginLiveRoom");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// ������ʾ��������...
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
		// ����json���ݰ�ʧ�ܣ����ñ�־����...
		if (!this->parseJson(strData, value, false)) {
			bIsError = true;
			break;
		}
		// ����������ȡ���Ĵ洢��������ַ�Ͷ˿�...
		string strTrackerAddr = CScreenApp::getJsonString(value["tracker_addr"]);
		string strTrackerPort = CScreenApp::getJsonString(value["tracker_port"]);
		// ����������ȡ����Զ����ת��������ַ�Ͷ˿�...
		string strRemoteAddr = CScreenApp::getJsonString(value["remote_addr"]);
		string strRemotePort = CScreenApp::getJsonString(value["remote_port"]);
		// ����������ȡ����udp��������ַ�Ͷ˿�...
		string strUdpAddr = CScreenApp::getJsonString(value["udp_addr"]);
		string strUdpPort = CScreenApp::getJsonString(value["udp_port"]);
		// ����������ȡ���ķ�����Ľ�ʦ��ѧ������...
		string strTeacherCount = CScreenApp::getJsonString(value["teacher"]);
		string strStudentCount = CScreenApp::getJsonString(value["student"]);
		if (strTrackerAddr.size() <= 0 || strTrackerPort.size() <= 0 || strRemoteAddr.size() <= 0 || strRemotePort.size() <= 0 ||
			strUdpAddr.size() <= 0 || strUdpPort.size() <= 0 || strTeacherCount.size() <= 0 || strStudentCount.size() <= 0) {
			m_strScan = QTStr("Screen.Room.Json");
			bIsError = true;
			break;
		}
		// ���㲢�жϷ�����Ľ�ʦ����������0�����ܵ�¼...
		int nTeacherCount = atoi(strTeacherCount.c_str());
		int nStudentCount = atoi(strStudentCount.c_str());
		blog(LOG_INFO, "Teacher: %d, Student: %d", nTeacherCount, nStudentCount);

		// ��ȡ��ֱ���ķֽ����ݣ�����ֱ����ַ���浽 => obs-screen/global.ini...
		config_set_string(GetGlobalConfig(), "General", "User", m_strUser.toStdString().c_str());
		config_set_string(GetGlobalConfig(), "General", "Room", m_strRoom.toStdString().c_str());
		config_set_string(GetGlobalConfig(), "General", "Pass", m_strPass.toStdString().c_str());
		// �������仯��������Ϣ���������ļ� => ��Ҫ���global.ini�ļ�...
		config_save_safe(GetGlobalConfig(), "tmp", nullptr);
		App()->SetRoomIDStr(m_strRoom.toStdString());
		App()->SetUserNameStr(m_strUser.toStdString());
		// ����ȡ������ص�ַ��Ϣ��ŵ�ȫ�ֶ�����...
		App()->SetTrackerAddr(strTrackerAddr);
		App()->SetTrackerPort(atoi(strTrackerPort.c_str()));
		App()->SetRemoteAddr(strRemoteAddr);
		App()->SetRemotePort(atoi(strRemotePort.c_str()));
		App()->SetUdpAddr(strUdpAddr);
		App()->SetUdpPort(atoi(strUdpPort.c_str()));
	} while (false);
	// �������󣬹رն�������ʾͼ��|��Ϣ�����������...
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
	// ����������ת�������ĻỰ����...
	this->doTcpConnRemote();
}

void CLoginMini::doTcpConnRemote()
{
	// �޸��������ķ�����״̬...
	m_eMiniState = kMiniRemoteConn;
	m_strScan = QStringLiteral("��������Զ�̷�����...");
	// �������ĻỰ���� => ����С������...
	ASSERT(m_RemoteSession == NULL);
	m_RemoteSession = new CRemoteSession();
	m_RemoteSession->InitSession(App()->GetRemoteAddr().c_str(), App()->GetRemotePort());
	// ע����ص��¼��������� => ͨ���źŲ���Ϣ�� => �����첽Ͷ��ģʽ...
	this->connect(m_RemoteSession, SIGNAL(doTriggerConnected()), this, SLOT(onTriggerConnected()), Qt::QueuedConnection);
	// ������ʾ��������...
	this->update();
}

// ��Ӧ���ĻỰ������¼�֪ͨ�źŲ�...
void CLoginMini::onTriggerConnected()
{
	// Զ���������˳���ֱ�ӷ���...
	if (m_RemoteSession == NULL)
		return;
	// ����Ϊ�˴ﵽ�첽Ч����ʹ����ʱ��...
	if (!m_RemoteSession->IsConnected()) {
		m_nFailedTimer = this->startTimer(10);
		return;
	}
	// ÿ��30����һ�Σ���ʦ�������ķ����������߻㱨֪ͨ...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// ������ʾ������Ϣ...
	m_strScan = QStringLiteral("���ڳ�ʼ����Ļץȡģ��...");
	this->update();
	// ������¼�ɹ�֮����¼�֪ͨ => �����첽֮�󣬱ܿ��˵ײ��߳�...
	emit this->doTriggerMiniSuccess();
}

/*void CLoginMini::doLoginSuccess()
{
	// �ر�����ʱ��...
	this->killTimer(m_nSuccessTimer);
	m_nSuccessTimer = -1;
	// ������ʾ������Ϣ...
	m_strScan = QStringLiteral("���ڳ�ʼ����Ļץȡģ��...");
	this->update();
	// ������¼�ɹ�֮����¼�֪ͨ => ����OBS...
	emit this->doTriggerMiniSuccess();
}*/

void CLoginMini::doLoginFailed()
{
	// �ر�����ʱ��...
	this->killTimer(m_nFailedTimer);
	m_nFailedTimer = -1;
	// ���½�����ʾԪ��...
	m_strScan = QString("%1%2").arg(QStringLiteral("����Զ�̷�����ʧ�ܣ�����ţ�")).arg(m_RemoteSession->GetErrCode());
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
	// ɾ��Զ�����Ӷ���...
	if (m_RemoteSession != NULL) {
		delete m_RemoteSession;
		m_RemoteSession = NULL;
	}
	// ���½���...
	this->update();
}

void CLoginMini::doSuccessOBS()
{
	// OBS��ʼ���ɹ�...
	QString strStyle = QString("background-image: url(:/mini/images/mini/scan.png);background-repeat: no-repeat;margin-left: 25px;margin-top: -46px;");
	ui->titleScan->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_strScan = QStringLiteral("�Ѿ��ɹ���¼�ƽ���ϵͳ...");
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
		m_strScan = QStringLiteral("������ȡ����JSON����ʧ��");
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
