
#include "window-login-main.h"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMouseEvent>
#include <QLineEdit>
#include <QPainter>
#include <QMovie>

#include <curl/curl.h>
#include <jansson.h>

CRoomItem::CRoomItem(int nRoomIndex, LoginWindow * lpWindow)
  : QLabel(lpWindow)
  , m_nDBRoomID(0)
  , m_bIsHover(false)
  , m_lpNetReply(NULL)
  , m_lpWindow(lpWindow)
  , m_nRoomIndex(nRoomIndex)
{
}

CRoomItem::~CRoomItem()
{
}

void CRoomItem::doDisplayItem(QPixmap & inPixmap)
{
	// 根据房间索引号计算房间所在的行号和列号...
	int nColVal = m_nRoomIndex % kColSize;
	int nRowVal = m_nRoomIndex / kColSize;
	// 计算背景图片的显示位置...
	int nXPos = kMarginSpace + nColVal * (kPosterWidth + kMarginSpace);
	int nYPos = kTitleSpace + kMarginSpace + nRowVal * (kPosterHeight + kMarginSpace);
	// 设置背景图片固定显示大小、鼠标、允许伸缩...
	this->setFixedSize(kPosterWidth, kPosterHeight);
	this->setCursor(Qt::PointingHandCursor);
	this->setScaledContents(true);
	// 背景图片不为空才设置到Label对象当中...
	if (!inPixmap.isNull()) {
		this->setPixmap(inPixmap);
	}
	// 移动Label对象到计算好的指定位置...
	this->move(nXPos, nYPos);
	this->raise();
}

void CRoomItem::paintEvent(QPaintEvent *event)
{
	// 构造系统绘制对象，采用路径绘制方式...
	QPainter painter(this);
	QPainterPath pathBack, pathMask;
	// 设置常规普通文本字体大小...
	// 注意：字体要设置给绘制对象...
	QFont theFont = this->font();
	theFont.setPointSize(kTextFontSize);
	painter.setFont(theFont);
	// 设置背景填充模式，消除锯齿模式...
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	// 如果是鼠标进入状态需要绘制红色边框，并修改背景填充的区域...
	// 注意：这里是填充模式，也可以用画边框模式，但是效果不太好...
	if (this->m_bIsHover) {
		painter.setBrush(QBrush(Qt::red));
		painter.setPen(Qt::transparent);
		painter.drawRoundedRect(this->rect(), kRadiusSize, kRadiusSize);
	}
	// 计算背景边框，默认背景边框为0...
	int nBorderSize = (this->m_bIsHover ? kBorderSize : 0);
	QRect rcRoundRect = QRect(nBorderSize, nBorderSize, this->width() - nBorderSize * 2, this->height() - nBorderSize * 2);
	pathBack.addRoundedRect(rcRoundRect, kRadiusSize, kRadiusSize);
	// 获取背景填充图片对象，如果无效，使用纯色填充...
	const QPixmap * lpBackPix = this->pixmap();
	painter.fillPath(pathBack, (lpBackPix == NULL) ? QBrush(Qt::lightGray) : QBrush(*lpBackPix));

	// 设置半透明蒙版透明度...
	painter.setOpacity(0.5);
	// 蒙板区域还是使用圆角路径绘制模式...
	rcRoundRect.setTop(rcRoundRect.bottom() - kMaskHeight + nBorderSize);
	pathMask.addRoundedRect(rcRoundRect, kRadiusSize, kRadiusSize);
	painter.fillPath(pathMask, QBrush(Qt::black));

	// 计算实际的直播房间编号 => 起始编号 + 数据库编号...
	int nLiveRoomID = m_lpWindow->GetRoomBeginID() + m_nDBRoomID;

	// 绘制房间里的文字内容信息...
	int nXPos = 10;
	int nYPos = rcRoundRect.top() + 25;
	painter.setOpacity(1.0);
	painter.setPen(QPen(this->m_bIsHover ? Qt::yellow : Qt::white));
	painter.drawText(nXPos, nYPos, QString("%1%2").arg(QStringLiteral("房间：")).arg(nLiveRoomID));
	//painter.drawText(nXPos, nYPos + 20, QString("%1%2").arg(QStringLiteral("名称：")).arg(m_strRoomName));
	painter.drawText(nXPos, nYPos + 40, QString("%1%2").arg(QStringLiteral("讲师：")).arg(m_strTeacherName));
	painter.drawText(nXPos, nYPos + 60, QString("%1%2").arg(QStringLiteral("时间：")).arg(m_strStartTime));

	// 可能越界的房间名称进行特殊显示处理...
	QFontMetrics fmPos = painter.fontMetrics();
	QRect rcNameRect(nXPos, nYPos + 5, rcRoundRect.width() - nXPos, 20);
	QString strRoomName = QString("%1%2").arg(QStringLiteral("名称：")).arg(m_strRoomName);
	QString strElidedText = fmPos.elidedText(strRoomName, Qt::ElideRight, rcNameRect.width(), Qt::TextShowMnemonic);
	painter.drawText(rcNameRect, Qt::AlignLeft, strElidedText);

	// 鼠标焦点有效时，需要额外居中绘制房间编号...
	/*if (this->m_bIsHover) {
		theFont.setPointSize(kRoomFontSize);
		painter.setFont(theFont);
		painter.setPen(QPen(Qt::yellow));
		painter.drawText(this->rect(), Qt::AlignCenter, QString("%1").arg(nLiveRoomID));
	}*/

	// 调用系统的事件绘制接口...
	QWidget::paintEvent(event);
}

void CRoomItem::enterEvent(QEvent *event)
{
	m_bIsHover = true;
	this->update();
}

void CRoomItem::leaveEvent(QEvent *event)
{
	m_bIsHover = false;
	this->update();
}

void CRoomItem::mousePressEvent(QMouseEvent *event)
{
	m_lpWindow->onClickRoomItem(m_nDBRoomID);
}

size_t procPostCurl(void *ptr, size_t size, size_t nmemb, void *stream)
{
	LoginWindow * lpWindow = (LoginWindow*)stream;
	lpWindow->doPostCurl((char*)ptr, size * nmemb);
	return size * nmemb;
}

LoginWindow::LoginWindow(QWidget *parent)
  : BaseWindow(parent)
  , m_nTotalNum(0)
  , m_nMaxPage(0)
  , m_nCurPage(0)
  , m_nBeginID(0)
  , m_isPressed(false)
  , m_lpLoadBack(NULL)
  , m_lpMovieGif(NULL)
  , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);
	this->initWindow();
	this->initMyTitle();
	this->initLoadGif();
	this->loadStyleSheet(":/login/css/LoginWindow.css");
}

LoginWindow::~LoginWindow()
{
	delete ui; ui = NULL;
	this->clearArrayRoom();
}

// 创建房间列表对象数组集合...
void LoginWindow::initArrayRoom()
{
	for (int i = 0; i < kPageRoom; ++i) {
		m_ArrayRoom[i] = new CRoomItem(i, this);
	}
}

// 释放映射集合里面的房间对象...
void LoginWindow::clearArrayRoom()
{
	for (int i = 0; i < kPageRoom; ++i) {
		delete m_ArrayRoom[i];
		m_ArrayRoom[i] = NULL;
	}
}

// 重置并隐藏所有的房间数组对象...
void LoginWindow::hideArrayRoom()
{
	for (int i = 0; i < kPageRoom; ++i) {
		m_ArrayRoom[i]->hide();
		m_ArrayRoom[i]->m_nDBRoomID = 0;
		m_ArrayRoom[i]->m_bIsHover = false;
		m_ArrayRoom[i]->m_lpNetReply = NULL;
	}
}

void LoginWindow::onTriggerRoomList(int nRoomPage)
{
	// 先将缓冲区进行清空处理...
	ui->errLabel->show();
	m_strUTF8Data.clear();
	this->hideArrayRoom();
	// 判断完毕，组合命令字符串...
	char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/getRoomList", App()->GetWebAddr().c_str());
	sprintf(szPost, "p=%d&type_id=%d", nRoomPage, App()->GetClientType());
	// 调用Curl接口，汇报命令信息...
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
	// 接口调用完毕，可以关闭加载框...
	m_lpLoadBack->hide();
	// 如果返回的数据为空，提示错误...
	if (m_strUTF8Data.size() <= 0) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.List"));
		ui->errLabel->setText(QTStr("Teacher.Room.List"));
		return;
	}
	// 开始解析传递过来的 json 数据包...
	json_error_t theError = { 0 };
	json_t * theRoot = json_loads(m_strUTF8Data.c_str(), 0, &theError);
	json_t * theCode = json_object_get(theRoot, "err_code");
	json_t * theMsg = json_object_get(theRoot, "err_msg");
	const char * lpStrMsg = json_string_value(theMsg);
	if (theCode == NULL || theMsg == NULL || lpStrMsg == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	if (theCode->type == JSON_TRUE) {
		QString strErrMsg = QString::fromUtf8(lpStrMsg);
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), strErrMsg);
		ui->errLabel->setText(strErrMsg);
		json_decref(theRoot);
		return;
	}
	// 将获取到的分页信息存储起来...
	string & strAddr = App()->GetWebAddr();
	json_t * theTotalNum = json_object_get(theRoot, "total_num");
	json_t * theMaxPage = json_object_get(theRoot, "max_page");
	json_t * theCurPage = json_object_get(theRoot, "cur_page");
	json_t * theBeginID = json_object_get(theRoot, "begin_id");
	json_t * theRoomList = json_object_get(theRoot, "list");
	m_nTotalNum = atoi(json_string_value(theTotalNum));
	m_nMaxPage = atoi(json_string_value(theMaxPage));
	m_nCurPage = atoi(json_string_value(theCurPage));
	m_nBeginID = atoi(json_string_value(theBeginID));
	// 更新标题栏内容信息...
	this->doUpdateTitle();
	// 如果没有获取到记录，直接返回...
	if (!json_is_array(theRoomList)) return;
	// 遍历获取到的房间记录...
	size_t index; json_t *value;
	json_array_foreach(theRoomList, index, value) {
		if (!json_is_object(value)) continue;
		json_t * theRoomID = json_object_get(value, "room_id");
		json_t * theRoomName = json_object_get(value, "room_name");
		json_t * theImageID = json_object_get(value, "image_id");
		json_t * theImageTag = json_object_get(value, "image_fdfs");
		json_t * thePosterID = json_object_get(value, "poster_id");
		json_t * thePosterTag = json_object_get(value, "poster_fdfs");
		json_t * theTeacherName = json_object_get(value, "teacher_name");
		json_t * theStartTime = json_object_get(value, "start_time");
		json_t * theEndTime = json_object_get(value, "end_time");
		// 计算截图连接地址和海报连接地址，海报地址无效时使用截图地址...
		int nDBPosterID = (json_is_integer(thePosterID)) ? json_integer_value(thePosterID) : atoi(json_string_value(thePosterID));
		int nDBImageID = (json_is_integer(theImageID)) ? json_integer_value(theImageID) : atoi(json_string_value(theImageID));
		int nDBRoomID = (json_is_integer(theRoomID)) ? json_integer_value(theRoomID) : atoi(json_string_value(theRoomID));
		QString strPosterTag = (nDBPosterID > 0) ? QString("%1/%2_200x300").arg(strAddr.c_str()).arg(json_string_value(thePosterTag)) : "";
		QString strImageTag = (nDBImageID > 0) ? QString("%1/%2_200x300").arg(strAddr.c_str()).arg(json_string_value(theImageTag)) : "";
		QString strRequestUrl = (strPosterTag.isEmpty() || strPosterTag.size() <= 0) ? strImageTag : strPosterTag;
		QString strTeacherName = json_string_value(theTeacherName);
		QString strRoomName = json_string_value(theRoomName);
		QString strStartTime = json_string_value(theStartTime);
		QString strEndTime = json_string_value(theEndTime);
		// 构造网络图片请求对象...
		QNetworkRequest theRequest;
		QNetworkReply * lpNetReply = NULL;
		// 每个海报都要创建这个比对凭证...
		theRequest.setUrl(QUrl(strRequestUrl));
		lpNetReply = m_objNetManager.get(theRequest);
		// 保存海报凭证，并将获取到的数据存放到对应房间数据单元当中，用于进一步的绘制使用...
		m_ArrayRoom[index]->m_nDBRoomID = nDBRoomID;
		m_ArrayRoom[index]->m_lpNetReply = lpNetReply;
		m_ArrayRoom[index]->m_strEndTime = strEndTime;
		m_ArrayRoom[index]->m_strRoomName = strRoomName;
		m_ArrayRoom[index]->m_strStartTime = strStartTime;
		m_ArrayRoom[index]->m_strPosterUrl = strRequestUrl;
		m_ArrayRoom[index]->m_strTeacherName = strTeacherName;
		// 根据索引，显示有效的房间对象...
		m_ArrayRoom[index]->show();
	}
	// 关闭错误信息显示框...
	ui->errLabel->hide();
}

void LoginWindow::replyFinished(QNetworkReply *reply)
{
	QPixmap thePixmap;
	// 网络正常读取网络数据，网络失败显示固定错误文件...
	if (reply->error() == QNetworkReply::NoError) {
		thePixmap.loadFromData(reply->readAll());
	}
	// 将海报图片和相关文字显示到房间条目当中...
	for (int i = 0; i < kPageRoom; ++i) {
		if (m_ArrayRoom[i]->m_lpNetReply == reply) {
			m_ArrayRoom[i]->doDisplayItem(thePixmap);
			break;
		}
	}
}

void LoginWindow::initLoadGif()
{
	// 先初始化房间列表...
	this->initArrayRoom();
	// 设置背景动画图片 => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/login/images/login/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	// 将动画移动到窗口居中显示...
	QRect rcRect = this->rect();
	int nXPos = rcRect.width() / 2 - 10;
	int nYPos = rcRect.height() / 2 - 20;
	m_lpLoadBack->move(nXPos, nYPos);
	// 设置获取房间分页列表的信号槽事件...
	connect(this, SIGNAL(doTriggerRoomList(int)), this, SLOT(onTriggerRoomList(int)));
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(replyFinished(QNetworkReply *)));
	// 直接启动异步的读取房间第一页列表的事件...
	emit this->doTriggerRoomList(1);
}

void LoginWindow::initMyTitle()
{
	// 因为这里有控件层叠了，所以要注意控件raise()方法的调用顺序;
	m_titleBar->move(0, 0);
	m_titleBar->raise();
	m_titleBar->setTitleWidth(this->width());
	m_titleBar->setBackgroundColor(QColor(0, 122, 204), false);
	// 这里需要设置成false，不允许通过标题栏拖动来移动窗口位置,否则会造成窗口位置错误;
	m_titleBar->setMoveParentWindowFlag(false);
	// 移动箭头按钮到最上层...
	ui->pButtonArrow->raise();
	// 更新标题内容...
	this->doUpdateTitle();
}

void LoginWindow::doUpdateTitle()
{
	QString strTitle = QString(QTStr("Login.Window.TitleContent")).arg(m_nCurPage).arg(m_nMaxPage).arg(m_nTotalNum);
	m_titleBar->setTitleContent(strTitle);
	m_titleBar->update();
}

void LoginWindow::initWindow()
{
	// 设置背景色 => 由静态图片代替了...
	//this->setBackGroundColor(QColor(198, 226, 255));
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
	// 隐藏登录相关按钮和编辑框...
	ui->accountComboBox->hide();
	ui->pButtonArrow->hide();
	ui->loginButton->hide();
	ui->userHead->hide();
	// 因此错误显示标签...
	ui->errLabel->hide();
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
	int nLiveRoomID = atoi(strRoomID.c_str());
	if (nLiveRoomID <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Number"));
		return;
	}
	// 调用服务器登录房间接口...
	this->doLoginAction(nLiveRoomID);
}
//
// 向远程服务器发送登录房间命令请求...
void LoginWindow::doLoginAction(int nLiveRoomID)
{
	// 显示错误信息框，并将数字转换成字符串...
	ui->errLabel->setText(""); ui->errLabel->show();
	string strLiveRoomID = QString("%1").arg(nLiveRoomID).toStdString();
	// 先将缓冲区进行清空处理...
	m_strUTF8Data.clear();
	// 判断完毕，将获取到的云教室号码发送到服务器验证...
	char  szUrl[MAX_PATH] = {0};
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/loginLiveRoom", App()->GetWebAddr().c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", strLiveRoomID.c_str(), App()->GetClientType());
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
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Empty"));
		ui->errLabel->setText(QTStr("Teacher.Room.Empty"));
		return;
	}
	// 开始解析传递过来的 json 数据包...
	json_error_t theError = { 0 };
	json_t * theRoot = json_loads(m_strUTF8Data.c_str(), 0, &theError);
	json_t * theCode = json_object_get(theRoot, "err_code");
	json_t * theMsg = json_object_get(theRoot, "err_msg");
	const char * lpStrMsg = json_string_value(theMsg);
	if (theCode == NULL || theMsg == NULL || lpStrMsg == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	if (theCode->type == JSON_TRUE) {
		QString strErrMsg = QString::fromUtf8(lpStrMsg);
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), strErrMsg);
		ui->errLabel->setText(strErrMsg);
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的存储服务器地址和端口...
	json_t * theTrackerAddr = json_object_get(theRoot, "tracker_addr");
	json_t * theTrackerPort = json_object_get(theRoot, "tracker_port");
	if (theTrackerAddr == NULL || theTrackerPort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的远程中转服务器地址和端口...
	json_t * theRemoteAddr = json_object_get(theRoot, "remote_addr");
	json_t * theRemotePort = json_object_get(theRoot, "remote_port");
	if (theRemoteAddr == NULL || theRemotePort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的udp服务器地址和端口...
	json_t * theUdpAddr = json_object_get(theRoot, "udp_addr");
	json_t * theUdpPort = json_object_get(theRoot, "udp_port");
	if (theUdpAddr == NULL || theUdpPort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 继续解析获取到的房间里的讲师和学生数量...
	json_t * theTeacherCount = json_object_get(theRoot, "teacher");
	json_t * theStudentCount = json_object_get(theRoot, "student");
	if (theTeacherCount == NULL || theStudentCount == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// 计算并判断房间里的讲师数量，大于0，不能登录...
	int nTeacherCount = atoi(json_string_value(theTeacherCount));
	int nStudentCount = atoi(json_string_value(theStudentCount));
	if (nTeacherCount > 0) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Login"));
		ui->errLabel->setText(QTStr("Teacher.Room.Login"));
		json_decref(theRoot);
		return;
	}
	// 没有错误，隐藏错误框...
	ui->errLabel->hide();
	// 获取到直播的分解数据，并将直播地址保存到 => obs-teacher/global.ini...
	config_set_string(App()->GlobalConfig(), "General", "LiveRoomID", strLiveRoomID.c_str());
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
	// 通知主进程主窗口可以启动了，传递房间号...
	emit this->loginSuccess(strLiveRoomID);
}
//
// 响应房间对象被点击事件...
void LoginWindow::onClickRoomItem(int nDBRoomID)
{
	int nLiveRoomID = m_nBeginID + nDBRoomID;
	this->doLoginAction(nLiveRoomID);
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
	painter.fillPath(pathBack, QBrush(QPixmap(":/login/images/login/back.jpg").scaled(this->size())));
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

// 滚轮事件移动窗口显示位置...
void LoginWindow::wheelEvent(QWheelEvent *event)
{
	// 滚动的角度，*8就是鼠标滚动的距离...
	// 滚动的步数，*15就是鼠标滚动的角度...
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;
	// 只处理垂直滚动事件...
	if (event->orientation() != Qt::Vertical)
		return;
	// 如果当前页数与数量本身就是无效的，直接返回...
	if (m_nMaxPage <= 0 || m_nCurPage <= 0 || m_nTotalNum <= 0)
		return;
	// 计算翻页数量，需要处理翻页越界的情况...
	int nCalcPage = m_nCurPage + numSteps;
	nCalcPage = (nCalcPage <= 1) ? 1 : nCalcPage;
	nCalcPage = (nCalcPage >= m_nMaxPage) ? m_nMaxPage : nCalcPage;
	// 如果计算后的页数与当前页一致，没有变化，直接返回...
	if (nCalcPage == m_nCurPage) return;
	// 调用接口进行新的一页房间列表读取，内部会清理界面...
	emit this->doTriggerRoomList(nCalcPage);
}

void LoginWindow::closeEvent(QCloseEvent *event)
{
	return __super::closeEvent(event);
}
