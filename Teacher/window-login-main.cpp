
#include "window-login-main.h"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include <QMouseEvent>
#include <QLineEdit>
#include <QPainter>

#include <curl/curl.h>
#include <jansson.h>

size_t procPostCurl(void *ptr, size_t size, size_t nmemb, void *stream)
{
	LoginWindow * lpWindow = (LoginWindow*)stream;
	lpWindow->doPostCurl((char*)ptr, size * nmemb);
	return size * nmemb;
}

LoginWindow::LoginWindow(QWidget *parent)
  : BaseWindow(parent)
  , m_isPressed(false)
  , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);
	this->initWindow();
	this->initMyTitle();
	this->loadStyleSheet(":/login/css/LoginWindow.css");
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::initMyTitle()
{
	// ��Ϊ�����пؼ�����ˣ�����Ҫע��ؼ�raise()�����ĵ���˳��;
	m_titleBar->move(0, 0);
	m_titleBar->raise();
	m_titleBar->setTitleWidth(this->width());
	m_titleBar->setTitleContent(QTStr("Login.Window.TitleContent"));
	m_titleBar->setBackgroundColor(QColor(0, 122, 204), false);
	// ������Ҫ���ó�false��������ͨ���������϶����ƶ�����λ��,�������ɴ���λ�ô���;
	m_titleBar->setMoveParentWindowFlag(false);
	// �ƶ���ͷ��ť�����ϲ�...
	ui->pButtonArrow->raise();
}

void LoginWindow::initWindow()
{
	// ���ñ���ɫ => �ɾ�̬ͼƬ������...
	//this->setBackGroundColor(QColor(198, 226, 255));
	// ���ñ�������ͼƬ => gif
	/*QLabel * pBack = new QLabel(this);
	QMovie * movie = new QMovie();
	movie->setFileName(":/login/images/login/back.gif");
	pBack->setMovie(movie);
	movie->start();
	pBack->move(0, 0);
	pBack->lower();*/
	// ���ô�������...
	Qt::WindowFlags qtFlag = this->windowFlags();
	this->setWindowFlags(qtFlag | Qt::WindowStaysOnTopHint);
	// ��ע�ͣ����޶��������ֵķ�Χ...
	ui->accountComboBox->setEditable(true);
	QLineEdit* lineEdit = ui->accountComboBox->lineEdit();
	lineEdit->setPlaceholderText(QTStr("Login.Window.EditHolderText"));
	lineEdit->setValidator(new QIntValidator(0, 999999999, this));
	lineEdit->setMaxLength(10);
	// �������ļ��ж�ȡ�ƽ��Һ��룬���õ��༭����...
	const char * lpLiveRoomID = config_get_string(App()->GlobalConfig(), "General", "LiveRoomID");
	if( lpLiveRoomID != NULL ) {
		lineEdit->setText(QString::fromUtf8(lpLiveRoomID));
	} else {
		ui->accountComboBox->setFocus();
	}
	// logoͼƬ��ʾλ��...
	ui->userHead->setPixmap(QPixmap(":/res/images/obs.png"));
	ui->userHead->setScaledContents(true);
	// ���ô���ͼ��...
	this->setWindowIcon(QIcon(":/res/images/obs.png"));
	// ���ð�ť�ĵ���¼�������...
	connect(ui->loginButton, SIGNAL(clicked()), this, SLOT(onClickLoginButton()));
	connect(ui->pButtonArrow, SIGNAL(clicked()), this, SLOT(onClickArrowButton()));
	connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
}
//
// ��ÿ�η��������ݽ����ۼ� => �п���һ������������Ƿֿ����͵�...
void LoginWindow::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE����г������ƣ�̫����ض�...
}
//
// ��Ӧ�����¼��ť...
void LoginWindow::onClickLoginButton()
{
	// �������ķ�����Ƿ���Ч...
	QLineEdit* lineEdit = ui->accountComboBox->lineEdit();
	string strRoomID = string((const char *)lineEdit->text().toLocal8Bit());
	// �ж�������ƽ��Һ����Ƿ�Ϊ��...
	if (strRoomID.size() <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.None"));
		return;
	}
	// �ж�������ƽ��Һ����Ƿ�Ϊ��Ч����...
	if (atoi(strRoomID.c_str()) <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Number"));
		return;
	}
	// �Ƚ�������������մ���...
	m_strUTF8Data.clear();
	// �ж���ϣ�����ȡ�����ƽ��Һ��뷢�͵���������֤...
	char  szUrl[MAX_PATH] = {0};
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/loginLiveRoom", App()->GetWebAddr().c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", strRoomID.c_str(), App()->GetClientType());
	// ����Curl�ӿڣ��㱨�ɼ�����Ϣ...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL)
			break;
		// �趨curl����������postģʽ������5�볬ʱ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// �ͷ���Դ...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	// ������ص�����Ϊ�գ���ʾ����...
	if( m_strUTF8Data.size() <= 0 ) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Empty"));
		return;
	}
	// ��ʼ�������ݹ����� json ���ݰ�...
	json_error_t theError = { 0 };
	json_t * theRoot = json_loads(m_strUTF8Data.c_str(), 0, &theError);
	json_t * theCode = json_object_get(theRoot, "err_code");
	json_t * theMsg = json_object_get(theRoot, "err_msg");
	const char * lpStrMsg = json_string_value(theMsg);
	if (theCode == NULL || theMsg == NULL || lpStrMsg == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	if (theCode->type == JSON_TRUE) {
		QString strErrMsg = QString::fromUtf8(lpStrMsg);
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), strErrMsg);
		json_decref(theRoot);
		return;
	}
	// ����������ȡ���Ĵ洢��������ַ�Ͷ˿�...
	json_t * theTrackerAddr = json_object_get(theRoot, "tracker_addr");
	json_t * theTrackerPort = json_object_get(theRoot, "tracker_port");
	if (theTrackerAddr == NULL || theTrackerPort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ����Զ����ת��������ַ�Ͷ˿�...
	json_t * theRemoteAddr = json_object_get(theRoot, "remote_addr");
	json_t * theRemotePort = json_object_get(theRoot, "remote_port");
	if (theRemoteAddr == NULL || theRemotePort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ����udp��������ַ�Ͷ˿�...
	json_t * theUdpAddr = json_object_get(theRoot, "udp_addr");
	json_t * theUdpPort = json_object_get(theRoot, "udp_port");
	if (theUdpAddr == NULL || theUdpPort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ���ķ�����Ľ�ʦ��ѧ������...
	json_t * theTeacherCount = json_object_get(theRoot, "teacher");
	json_t * theStudentCount = json_object_get(theRoot, "student");
	if (theTeacherCount == NULL || theStudentCount == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ���㲢�жϷ�����Ľ�ʦ����������0�����ܵ�¼...
	int nTeacherCount = atoi(json_string_value(theTeacherCount));
	int nStudentCount = atoi(json_string_value(theStudentCount));
	if (nTeacherCount > 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Login"));
		json_decref(theRoot);
		return;
	}
	// ��ȡ��ֱ���ķֽ����ݣ�����ֱ����ַ���浽 => obs-teacher/global.ini...
	config_set_string(App()->GlobalConfig(), "General", "LiveRoomID", strRoomID.c_str());
	//config_set_string(App()->GlobalConfig(), "General", "TrackerAddr", lpTrackerAddr);
	//config_set_string(App()->GlobalConfig(), "General", "TrackerPort", lpTrackerPort);
	//config_set_string(App()->GlobalConfig(), "General", "RemoteAddr", lpRemoteAddr);
	//config_set_string(App()->GlobalConfig(), "General", "RemotePort", lpRemotePort);
	//config_set_string(App()->GlobalConfig(), "General", "UdpAddr", lpUdpAddr);
	//config_set_string(App()->GlobalConfig(), "General", "UdpPort", lpUdpPort);
	// ����ȡ������ص�ַ��Ϣ��ŵ�ȫ�ֶ�����...
	const char * lpTrackerAddr = json_string_value(theTrackerAddr);
	const char * lpTrackerPort = json_string_value(theTrackerPort);
	const char * lpRemoteAddr = json_string_value(theRemoteAddr);
	const char * lpRemotePort = json_string_value(theRemotePort);
	const char * lpUdpAddr = json_string_value(theUdpAddr);
	const char * lpUdpPort = json_string_value(theUdpPort);
	App()->SetTrackerAddr(string(lpTrackerAddr));
	App()->SetTrackerPort(atoi(lpTrackerPort));
	App()->SetRemoteAddr(string(lpRemoteAddr));
	App()->SetRemotePort(atoi(lpRemotePort));
	App()->SetUdpAddr(string(lpUdpAddr));
	App()->SetUdpPort(atoi(lpUdpPort));
	// ���ݴ�����ϣ��ͷ�json���ݰ�...
	json_decref(theRoot);
	// ֪ͨ�����������ڿ���������...
	emit this->loginSuccess(strRoomID);
}
//
// ��Ӧ�����ͷ��ť...
void LoginWindow::onClickArrowButton()
{
}
//
// ����Բ�Ǵ��ڱ���...
void LoginWindow::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QPainterPath pathBack;
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	pathBack.addRoundedRect(QRect(0, 0, this->width(), this->height()), 4, 4);
	painter.fillPath(pathBack, QBrush(QPixmap(":/login/images/login/back.jpg")));
	QWidget::paintEvent(event);
}
//
// ����ͨ�� mousePressEvent | mouseMoveEvent | mouseReleaseEvent �����¼�ʵ��������϶��������ƶ����ڵ�Ч��;
void LoginWindow::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		m_isPressed = true;
		m_startMovePos = event->globalPos();
	}
	return QWidget::mousePressEvent(event);
}

void LoginWindow::mouseMoveEvent(QMouseEvent *event)
{
	if (m_isPressed) {
		QPoint movePoint = event->globalPos() - m_startMovePos;
		QPoint widgetPos = this->pos() + movePoint;
		m_startMovePos = event->globalPos();
		this->move(widgetPos.x(), widgetPos.y());
	}
	return QWidget::mouseMoveEvent(event);
}

void LoginWindow::mouseReleaseEvent(QMouseEvent *event)
{
	m_isPressed = false;
	return QWidget::mouseReleaseEvent(event);
}

void LoginWindow::closeEvent(QCloseEvent *event)
{
	return __super::closeEvent(event);
}
