
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
	// ��ʾС�����ά��...
	if (!m_QPixQRCode.isNull() && m_QPixQRCode.width() > 0) {
		int nXPos = (this->size().width() - m_QPixQRCode.size().width()) / 2;
		int nYPos = ui->hori_title->totalSizeHint().height();
		painter.drawPixmap(nXPos, nYPos, m_QPixQRCode);
	}
	// ��ʾ�汾|ɨ��|����...
	ui->titleVer->setText(m_strVer);
	ui->titleScan->setText(m_strScan);
	ui->titleQR->setText(m_strQRNotice);

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
	this->resize(350, 420);
	// ����PTZ���ڵĲ����ʾ��ʽ��...
	this->loadStyleSheet(":/mini/css/LoginMini.css");
	// ��ʼ���汾������Ϣ...
	m_strVer = QString("V%1").arg(OBS_VERSION);
	// ���ñ�������ͼƬ => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/mini/images/mini/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	// �������ƶ������ھ�����ʾ...
	QRect rcRect = this->rect();
	QRect rcSize = m_lpMovieGif->frameRect();
	int nXPos = (rcRect.width() - rcSize.width()) / 2;
	int nYPos = (rcRect.height() - rcSize.height()) / 2 - ui->btnClose->height() * 2 + 5;
	m_lpLoadBack->move(nXPos, nYPos);
	// ���������С����ť|�رհ�ť���źŲ��¼�...
	connect(ui->btnMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	// ���������źŲ۷�������¼�...
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onReplyFinished(QNetworkReply *)));
	// �����ȡ���ķ�������TCP��ַ�Ͷ˿ڵ�����...
	this->doWebGetCenterAddr();
}

void CLoginMini::doWebGetCenterAddr()
{
	// ���ĻỰ������Ч����Ҫ�ؽ�֮...
	if (m_CenterSession != NULL) {
		delete m_CenterSession;
		m_CenterSession = NULL;
	}
	// ��ʾ���ض���...
	m_lpLoadBack->show();
	// ɾ���������Ӽ��ʱ�Ӷ���...
	if (m_nCenterTimer != -1) {
		this->killTimer(m_nCenterTimer);
		m_nCenterTimer = -1;
	}
	// ɾ����������������������...
	if (m_nOnLineTimer != -1) {
		this->killTimer(m_nOnLineTimer);
		m_nOnLineTimer = -1;
	}
	// �޸Ļ�ȡ���ķ�����TCP��ַ״̬...
	m_eMiniState = kCenterAddr;
	m_strQRNotice = QStringLiteral("���ڻ�ȡ���ķ�������ַ...");
	// ����ƾ֤���ʵ�ַ��������������...
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	string & strWebCenter = App()->GetWebCenter();
	QString strRequestURL = QString("%1%2").arg(strWebCenter.c_str()).arg("/wxapi.php/Mini/getUDPCenter");
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	lpNetReply = m_objNetManager.get(theQTNetRequest);
	// ������ʾ��������...
	this->update();
}

void CLoginMini::doWebGetMiniToken()
{
	// �޸Ļ�ȡС����token״̬...
	m_eMiniState = kMiniToken;
	m_strQRNotice = QStringLiteral("���ڻ�ȡ����ƾ֤...");
	// ����ƾ֤���ʵ�ַ��������������...
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
	// ������ʾ��������...
	this->update();
}

void CLoginMini::doWebGetMiniQRCode()
{
	// �ж�access_token��path�Ƿ���Ч...
	if (m_strMiniPath.size() <= 0 || m_strMiniToken.size() <= 0) {
		m_strQRNotice = QStringLiteral("��ȡ����ƾ֤ʧ�ܣ��޷���ȡС������");
		this->update();
		return;
	}
	// �޸Ļ�ȡС�����ά��״̬...
	m_strQRNotice = QStringLiteral("���ڻ�ȡС������...");
	m_eMiniState = kMiniQRCode;
	QNetworkReply * lpNetReply = NULL;
	QNetworkRequest theQTNetRequest;
	// ׼����Ҫ�Ļ㱨���� => POST���ݰ�...
	Json::Value itemData;
	itemData["width"] = kQRCodeWidth;
	itemData["page"] = m_strMiniPath.c_str();
	itemData["scene"] = QString("teacher_%1").arg(m_nTcpSocketFD).toStdString();
	QString strContentVal = QString("%1").arg(itemData.toStyledString().c_str());
	QString strRequestURL = QString("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%1").arg(m_strMiniToken.c_str());
	theQTNetRequest.setUrl(QUrl(strRequestURL));
	theQTNetRequest.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	lpNetReply = m_objNetManager.post(theQTNetRequest, strContentVal.toUtf8());
	// ������ʾ��������...
	this->update();
}

void CLoginMini::onReplyFinished(QNetworkReply *reply)
{
	// �������������󣬴�ӡ������Ϣ������ѭ��...
	if (reply->error() != QNetworkReply::NoError) {
		blog(LOG_INFO, "QT error => %d, %s", reply->error(), reply->errorString().toStdString().c_str());
		m_strQRNotice = QString("%1, %2").arg(reply->error()).arg(reply->errorString());
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// ��ȡ�������������󷵻ص��������ݰ�...
	int nStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	blog(LOG_INFO, "QT Status Code => %d", nStatusCode);
	// ����״̬�ַ��������...
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
	// ����json�ɹ�������UDPCenter��TCP��ַ�Ͷ˿�...
	m_strCenterTcpAddr = OBSApp::getJsonString(value["udpcenter_addr"]);
	m_nCenterTcpPort = atoi(OBSApp::getJsonString(value["udpcenter_port"]).c_str());
	// �����������ķ������ĻỰ����...
	this->doTcpConnCenterAddr();
}

void CLoginMini::doTcpConnCenterAddr()
{
	// �޸��������ķ�����״̬...
	m_eMiniState = kCenterConn;
	m_strQRNotice = QStringLiteral("�����������ķ�����...");
	// �������ĻỰ���� => ����С������...
	ASSERT(m_CenterSession == NULL);
	m_CenterSession = new CCenterSession();
	m_CenterSession->InitSession(m_strCenterTcpAddr.c_str(), m_nCenterTcpPort);
	// ע����ص��¼��������� => ͨ���źŲ���Ϣ��...
	this->connect(m_CenterSession, SIGNAL(doTriggerTcpConnect()), this, SLOT(onTriggerTcpConnect()));
	// ����һ����ʱ�ؽ����ʱ�� => ÿ��5��ִ��һ��...
	m_nCenterTimer = this->startTimer(5 * 1000);
	// ������ʾ��������...
	this->update();
}

// ��Ӧ���ĻỰ������¼�֪ͨ�źŲ�...
void CLoginMini::onTriggerTcpConnect()
{
	// ����ʧ�ܣ���ӡ������Ϣ...
	ASSERT(m_CenterSession != NULL);
	if (!m_CenterSession->IsConnected()) {
		m_strQRNotice = QString("%1%2").arg(QStringLiteral("�������ķ�����ʧ�ܣ�����ţ�")).arg(m_CenterSession->GetErrCode());
		blog(LOG_INFO, "QT error => %d", m_CenterSession->GetErrCode());
		m_eMiniState = kCenterAddr;
		m_lpLoadBack->hide();
		m_strScan.clear();
		this->update();
		return;
	}
	// ������������������ķ�����״̬��ֱ�ӷ���...
	if (m_eMiniState != kCenterConn)
		return;
	ASSERT(m_eMiniState == kCenterConn);
	// �����ĻỰ�ڷ������ϵ��׽��ֱ�Ž��б���...
	m_nTcpSocketFD = m_CenterSession->GetTcpSocketFD();
	// ÿ��30����һ�Σ���ʦ�������ķ����������߻㱨֪ͨ...
	m_nOnLineTimer = this->startTimer(30 * 1000);
	// �����ȡС����Tokenֵ����������...
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
	// ������ĻỰ������Ч��Ͽ��������ȡ���ķ�������TCP��ַ�Ͷ˿ڵ�����...
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
	// ����ǻ�ȡtoken״̬��������ȡ��json����...
	blog(LOG_DEBUG, "QT Reply Data => %s", strData.c_str());
	if (!this->parseJson(strData, value, false)) {
		m_lpLoadBack->hide();
		this->update();
		return;
	}
	// ����json�ɹ�������token��·��...
	m_strMiniToken = OBSApp::getJsonString(value["access_token"]);
	m_strMiniPath = OBSApp::getJsonString(value["mini_path"]);
	// �����ȡС�����ά��Ĳ���...
	this->doWebGetMiniQRCode();
}

void CLoginMini::onProcMiniQRCode(QNetworkReply *reply)
{
	// ���۶Դ���Ҫ�رռ��ض���...
	m_lpLoadBack->hide();
	// ��ȡ��ȡ����������������...
	ASSERT(m_eMiniState == kMiniQRCode);
	QByteArray & theByteArray = reply->readAll();
	// ����·����ֱ�ӹ����ά���ͼƬ��Ϣ|�趨Ĭ��ɨ����Ϣ...
	if (m_QPixQRCode.loadFromData(theByteArray)) {
		m_strScan = QStringLiteral("����΢��ɨ���¼");
		m_strQRNotice.clear();
		this->update();
		return;
	}
	// ���λͼ��ʾ����...
	Json::Value value;
	m_QPixQRCode.detach();
	// �����ά��ͼƬ����ʧ�ܣ����д������...
	string & strData = theByteArray.toStdString();
	if (!this->parseJson(strData, value, true)) {
		this->update();
		return;
	}
	// ����json���ݰ���Ȼʧ�ܣ���ʾ�ض��Ĵ�����Ϣ...
	m_strQRNotice = QStringLiteral("������ȡ����JSON����ʧ��");
	this->update();
}

bool CLoginMini::parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin)
{
	Json::Reader reader;
	if (!reader.parse(inData, outValue)) {
		m_strQRNotice = QStringLiteral("������ȡ����JSON����ʧ��");
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
