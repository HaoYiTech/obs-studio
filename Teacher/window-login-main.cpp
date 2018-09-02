
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
	// 因为这里有控件层叠了，所以要注意控件raise()方法的调用顺序;
	m_titleBar->move(0, 0);
	m_titleBar->raise();
	m_titleBar->setTitleWidth(this->width());
	m_titleBar->setTitleContent(QTStr("Login.Window.TitleContent"));
	m_titleBar->setBackgroundColor(QColor(0, 122, 204), false);
	// 这里需要设置成false，不允许通过标题栏拖动来移动窗口位置,否则会造成窗口位置错误;
	m_titleBar->setMoveParentWindowFlag(false);
	// 移动箭头按钮到最上层...
	ui->pButtonArrow->raise();
}

void LoginWindow::initWindow()
{
	// 设置背景色 => 由静态图片代替了...
	//this->setBackGroundColor(QColor(198, 226, 255));
	// 设置背景动画图片 => gif
	/*QLabel * pBack = new QLabel(this);
	QMovie * movie = new QMovie();
	movie->setFileName(":/login/images/login/back.gif");
	pBack->setMovie(movie);
	movie->start();
	pBack->move(0, 0);
	pBack->lower();*/
	// 设置窗口属性...
	Qt::WindowFlags qtFlag = this->windowFlags();
	this->setWindowFlags(qtFlag | Qt::WindowStaysOnTopHint);
	// 暗注释，并限定输入数字的范围...
	ui->accountComboBox->setEditable(true);
	QLineEdit* lineEdit = ui->accountComboBox->lineEdit();
	lineEdit->setPlaceholderText(QTStr("Login.Window.EditHolderText"));
	lineEdit->setValidator(new QIntValidator(0, 999999999, this));
	lineEdit->setMaxLength(10);
	// 从配置文件中读取云教室号码，设置到编辑框当中...
	const char * lpLiveRoomID = config_get_string(App()->GlobalConfig(), "General", "LiveRoomID");
	if( lpLiveRoomID != NULL ) {
		lineEdit->setText(QString::fromUtf8(lpLiveRoomID));
	} else {
		ui->accountComboBox->setFocus();
	}
	// logo图片显示位置...
	ui->userHead->setPixmap(QPixmap(":/res/images/obs.png"));
	ui->userHead->setScaledContents(true);
	// 设置窗口图标...
	this->setWindowIcon(QIcon(":/res/images/obs.png"));
	// 设置按钮的点击事件处理函数...
	connect(ui->loginButton, SIGNAL(clicked()), this, SLOT(onClickLoginButton()));
	connect(ui->pButtonArrow, SIGNAL(clicked()), this, SLOT(onClickArrowButton()));
	connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(onClickLoginButton()));
}
//
// 将每次反馈的数据进行累加 => 有可能一次请求的数据是分开发送的...
void LoginWindow::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE输出有长度限制，太长会截断...
}
//
// 响应点击登录按钮...
void LoginWindow::onClickLoginButton()
{
	// 检测输入的房间号是否有效...
	QLineEdit* lineEdit = ui->accountComboBox->lineEdit();
	string strRoomID = string((const char *)lineEdit->text().toLocal8Bit());
	// 判断输入的云教室号码是否不为空...
	if (strRoomID.size() <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.None"));
		return;
	}
	// 判断输入的云教室号码是否为有效数字...
	if (atoi(strRoomID.c_str()) <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Number"));
		return;
	}
	// 先将缓冲区进行清空处理...
	m_strUTF8Data.clear();
	// 判断完毕，将获取到的云教室号码发送到服务器验证...
	char  szUrl[MAX_PATH] = {0};
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/loginLiveRoom", App()->GetWebAddr().c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", strRoomID.c_str(), App()->GetClientType());
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL)
			break;
		// 设定curl参数，采用post模式，设置5秒超时...
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
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	// 如果返回的数据为空，提示错误...
	if( m_strUTF8Data.size() <= 0 ) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Empty"));
		return;
	}
	// 开始解析传递过来的 json 数据包...
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
	// 继续解析获取到的存储服务器地址和端口...
	json_t * theTrackerAddr = json_object_get(theRoot, "tracker_addr");
	json_t * theTrackerPort = json_object_get(theRoot, "tracker_port");
	if (theTrackerAddr == NULL || theTrackerPort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的远程中转服务器地址和端口...
	json_t * theRemoteAddr = json_object_get(theRoot, "remote_addr");
	json_t * theRemotePort = json_object_get(theRoot, "remote_port");
	if (theRemoteAddr == NULL || theRemotePort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的udp服务器地址和端口...
	json_t * theUdpAddr = json_object_get(theRoot, "udp_addr");
	json_t * theUdpPort = json_object_get(theRoot, "udp_port");
	if (theUdpAddr == NULL || theUdpPort == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的房间里的讲师和学生数量...
	json_t * theTeacherCount = json_object_get(theRoot, "teacher");
	json_t * theStudentCount = json_object_get(theRoot, "student");
	if (theTeacherCount == NULL || theStudentCount == NULL) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 计算并判断房间里的讲师数量，大于0，不能登录...
	int nTeacherCount = atoi(json_string_value(theTeacherCount));
	int nStudentCount = atoi(json_string_value(theStudentCount));
	if (nTeacherCount > 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Login"));
		json_decref(theRoot);
		return;
	}
	// 获取到直播的分解数据，并将直播地址保存到 => obs-teacher/global.ini...
	config_set_string(App()->GlobalConfig(), "General", "LiveRoomID", strRoomID.c_str());
	//config_set_string(App()->GlobalConfig(), "General", "TrackerAddr", lpTrackerAddr);
	//config_set_string(App()->GlobalConfig(), "General", "TrackerPort", lpTrackerPort);
	//config_set_string(App()->GlobalConfig(), "General", "RemoteAddr", lpRemoteAddr);
	//config_set_string(App()->GlobalConfig(), "General", "RemotePort", lpRemotePort);
	//config_set_string(App()->GlobalConfig(), "General", "UdpAddr", lpUdpAddr);
	//config_set_string(App()->GlobalConfig(), "General", "UdpPort", lpUdpPort);
	// 将获取到的相关地址信息存放到全局对象当中...
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
	// 数据处理完毕，释放json数据包...
	json_decref(theRoot);
	// 通知主进程主窗口可以启动了...
	emit this->loginSuccess(strRoomID);
}
//
// 响应点击箭头按钮...
void LoginWindow::onClickArrowButton()
{
}
//
// 绘制圆角窗口背景...
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
// 以下通过 mousePressEvent | mouseMoveEvent | mouseReleaseEvent 三个事件实现了鼠标拖动标题栏移动窗口的效果;
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
