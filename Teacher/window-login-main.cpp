
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
	// ���ݷ��������ż��㷿�����ڵ��кź��к�...
	int nColVal = m_nRoomIndex % kColSize;
	int nRowVal = m_nRoomIndex / kColSize;
	// ���㱳��ͼƬ����ʾλ��...
	int nXPos = kMarginSpace + nColVal * (kPosterWidth + kMarginSpace);
	int nYPos = kTitleSpace + kMarginSpace + nRowVal * (kPosterHeight + kMarginSpace);
	// ���ñ���ͼƬ�̶���ʾ��С����ꡢ��������...
	this->setFixedSize(kPosterWidth, kPosterHeight);
	this->setCursor(Qt::PointingHandCursor);
	this->setScaledContents(true);
	// ����ͼƬ��Ϊ�ղ����õ�Label������...
	if (!inPixmap.isNull()) {
		this->setPixmap(inPixmap);
	}
	// �ƶ�Label���󵽼���õ�ָ��λ��...
	this->move(nXPos, nYPos);
	this->raise();
}

void CRoomItem::paintEvent(QPaintEvent *event)
{
	// ����ϵͳ���ƶ��󣬲���·�����Ʒ�ʽ...
	QPainter painter(this);
	QPainterPath pathBack, pathMask;
	// ���ó�����ͨ�ı������С...
	// ע�⣺����Ҫ���ø����ƶ���...
	QFont theFont = this->font();
	theFont.setPointSize(kTextFontSize);
	painter.setFont(theFont);
	// ���ñ������ģʽ���������ģʽ...
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	// �����������״̬��Ҫ���ƺ�ɫ�߿򣬲��޸ı�����������...
	// ע�⣺���������ģʽ��Ҳ�����û��߿�ģʽ������Ч����̫��...
	if (this->m_bIsHover) {
		painter.setBrush(QBrush(Qt::red));
		painter.setPen(Qt::transparent);
		painter.drawRoundedRect(this->rect(), kRadiusSize, kRadiusSize);
	}
	// ���㱳���߿�Ĭ�ϱ����߿�Ϊ0...
	int nBorderSize = (this->m_bIsHover ? kBorderSize : 0);
	QRect rcRoundRect = QRect(nBorderSize, nBorderSize, this->width() - nBorderSize * 2, this->height() - nBorderSize * 2);
	pathBack.addRoundedRect(rcRoundRect, kRadiusSize, kRadiusSize);
	// ��ȡ�������ͼƬ���������Ч��ʹ�ô�ɫ���...
	const QPixmap * lpBackPix = this->pixmap();
	painter.fillPath(pathBack, (lpBackPix == NULL) ? QBrush(Qt::lightGray) : QBrush(*lpBackPix));

	// ���ð�͸���ɰ�͸����...
	painter.setOpacity(0.5);
	// �ɰ�������ʹ��Բ��·������ģʽ...
	rcRoundRect.setTop(rcRoundRect.bottom() - kMaskHeight + nBorderSize);
	pathMask.addRoundedRect(rcRoundRect, kRadiusSize, kRadiusSize);
	painter.fillPath(pathMask, QBrush(Qt::black));

	// ����ʵ�ʵ�ֱ�������� => ��ʼ��� + ���ݿ���...
	int nLiveRoomID = m_lpWindow->GetRoomBeginID() + m_nDBRoomID;

	// ���Ʒ����������������Ϣ...
	int nXPos = 10;
	int nYPos = rcRoundRect.top() + 25;
	painter.setOpacity(1.0);
	painter.setPen(QPen(this->m_bIsHover ? Qt::yellow : Qt::white));
	painter.drawText(nXPos, nYPos, QString("%1%2").arg(QStringLiteral("���䣺")).arg(nLiveRoomID));
	//painter.drawText(nXPos, nYPos + 20, QString("%1%2").arg(QStringLiteral("���ƣ�")).arg(m_strRoomName));
	painter.drawText(nXPos, nYPos + 40, QString("%1%2").arg(QStringLiteral("��ʦ��")).arg(m_strTeacherName));
	painter.drawText(nXPos, nYPos + 60, QString("%1%2").arg(QStringLiteral("ʱ�䣺")).arg(m_strStartTime));

	// ����Խ��ķ������ƽ���������ʾ����...
	QFontMetrics fmPos = painter.fontMetrics();
	QRect rcNameRect(nXPos, nYPos + 5, rcRoundRect.width() - nXPos, 20);
	QString strRoomName = QString("%1%2").arg(QStringLiteral("���ƣ�")).arg(m_strRoomName);
	QString strElidedText = fmPos.elidedText(strRoomName, Qt::ElideRight, rcNameRect.width(), Qt::TextShowMnemonic);
	painter.drawText(rcNameRect, Qt::AlignLeft, strElidedText);

	// ��꽹����Чʱ����Ҫ������л��Ʒ�����...
	/*if (this->m_bIsHover) {
		theFont.setPointSize(kRoomFontSize);
		painter.setFont(theFont);
		painter.setPen(QPen(Qt::yellow));
		painter.drawText(this->rect(), Qt::AlignCenter, QString("%1").arg(nLiveRoomID));
	}*/

	// ����ϵͳ���¼����ƽӿ�...
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

// ���������б�������鼯��...
void LoginWindow::initArrayRoom()
{
	for (int i = 0; i < kPageRoom; ++i) {
		m_ArrayRoom[i] = new CRoomItem(i, this);
	}
}

// �ͷ�ӳ�伯������ķ������...
void LoginWindow::clearArrayRoom()
{
	for (int i = 0; i < kPageRoom; ++i) {
		delete m_ArrayRoom[i];
		m_ArrayRoom[i] = NULL;
	}
}

// ���ò��������еķ����������...
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
	// �Ƚ�������������մ���...
	ui->errLabel->show();
	m_strUTF8Data.clear();
	this->hideArrayRoom();
	// �ж���ϣ���������ַ���...
	char  szUrl[MAX_PATH] = { 0 };
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/getRoomList", App()->GetWebAddr().c_str());
	sprintf(szPost, "p=%d&type_id=%d", nRoomPage, App()->GetClientType());
	// ����Curl�ӿڣ��㱨������Ϣ...
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
	// �ӿڵ�����ϣ����Թرռ��ؿ�...
	m_lpLoadBack->hide();
	// ������ص�����Ϊ�գ���ʾ����...
	if (m_strUTF8Data.size() <= 0) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.List"));
		ui->errLabel->setText(QTStr("Teacher.Room.List"));
		return;
	}
	// ��ʼ�������ݹ����� json ���ݰ�...
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
	// ����ȡ���ķ�ҳ��Ϣ�洢����...
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
	// ���±�����������Ϣ...
	this->doUpdateTitle();
	// ���û�л�ȡ����¼��ֱ�ӷ���...
	if (!json_is_array(theRoomList)) return;
	// ������ȡ���ķ����¼...
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
		// �����ͼ���ӵ�ַ�ͺ������ӵ�ַ��������ַ��Чʱʹ�ý�ͼ��ַ...
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
		// ��������ͼƬ�������...
		QNetworkRequest theRequest;
		QNetworkReply * lpNetReply = NULL;
		// ÿ��������Ҫ��������ȶ�ƾ֤...
		theRequest.setUrl(QUrl(strRequestUrl));
		lpNetReply = m_objNetManager.get(theRequest);
		// ���溣��ƾ֤��������ȡ�������ݴ�ŵ���Ӧ�������ݵ�Ԫ���У����ڽ�һ���Ļ���ʹ��...
		m_ArrayRoom[index]->m_nDBRoomID = nDBRoomID;
		m_ArrayRoom[index]->m_lpNetReply = lpNetReply;
		m_ArrayRoom[index]->m_strEndTime = strEndTime;
		m_ArrayRoom[index]->m_strRoomName = strRoomName;
		m_ArrayRoom[index]->m_strStartTime = strStartTime;
		m_ArrayRoom[index]->m_strPosterUrl = strRequestUrl;
		m_ArrayRoom[index]->m_strTeacherName = strTeacherName;
		// ������������ʾ��Ч�ķ������...
		m_ArrayRoom[index]->show();
	}
	// �رմ�����Ϣ��ʾ��...
	ui->errLabel->hide();
}

void LoginWindow::replyFinished(QNetworkReply *reply)
{
	QPixmap thePixmap;
	// ����������ȡ�������ݣ�����ʧ����ʾ�̶������ļ�...
	if (reply->error() == QNetworkReply::NoError) {
		thePixmap.loadFromData(reply->readAll());
	}
	// ������ͼƬ�����������ʾ��������Ŀ����...
	for (int i = 0; i < kPageRoom; ++i) {
		if (m_ArrayRoom[i]->m_lpNetReply == reply) {
			m_ArrayRoom[i]->doDisplayItem(thePixmap);
			break;
		}
	}
}

void LoginWindow::initLoadGif()
{
	// �ȳ�ʼ�������б�...
	this->initArrayRoom();
	// ���ñ�������ͼƬ => gif
	m_lpMovieGif = new QMovie();
	m_lpLoadBack = new QLabel(this);
	m_lpMovieGif->setFileName(":/login/images/login/loading.gif");
	m_lpLoadBack->setMovie(m_lpMovieGif);
	m_lpMovieGif->start();
	m_lpLoadBack->raise();
	// �������ƶ������ھ�����ʾ...
	QRect rcRect = this->rect();
	int nXPos = rcRect.width() / 2 - 10;
	int nYPos = rcRect.height() / 2 - 20;
	m_lpLoadBack->move(nXPos, nYPos);
	// ���û�ȡ�����ҳ�б���źŲ��¼�...
	connect(this, SIGNAL(doTriggerRoomList(int)), this, SLOT(onTriggerRoomList(int)));
	connect(&m_objNetManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(replyFinished(QNetworkReply *)));
	// ֱ�������첽�Ķ�ȡ�����һҳ�б���¼�...
	emit this->doTriggerRoomList(1);
}

void LoginWindow::initMyTitle()
{
	// ��Ϊ�����пؼ�����ˣ�����Ҫע��ؼ�raise()�����ĵ���˳��;
	m_titleBar->move(0, 0);
	m_titleBar->raise();
	m_titleBar->setTitleWidth(this->width());
	m_titleBar->setBackgroundColor(QColor(0, 122, 204), false);
	// ������Ҫ���ó�false��������ͨ���������϶����ƶ�����λ��,�������ɴ���λ�ô���;
	m_titleBar->setMoveParentWindowFlag(false);
	// �ƶ���ͷ��ť�����ϲ�...
	ui->pButtonArrow->raise();
	// ���±�������...
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
	// ���ñ���ɫ => �ɾ�̬ͼƬ������...
	//this->setBackGroundColor(QColor(198, 226, 255));
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
	// ���ص�¼��ذ�ť�ͱ༭��...
	ui->accountComboBox->hide();
	ui->pButtonArrow->hide();
	ui->loginButton->hide();
	ui->userHead->hide();
	// ��˴�����ʾ��ǩ...
	ui->errLabel->hide();
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
	int nLiveRoomID = atoi(strRoomID.c_str());
	if (nLiveRoomID <= 0) {
		OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Number"));
		return;
	}
	// ���÷�������¼����ӿ�...
	this->doLoginAction(nLiveRoomID);
}
//
// ��Զ�̷��������͵�¼������������...
void LoginWindow::doLoginAction(int nLiveRoomID)
{
	// ��ʾ������Ϣ�򣬲�������ת�����ַ���...
	ui->errLabel->setText(""); ui->errLabel->show();
	string strLiveRoomID = QString("%1").arg(nLiveRoomID).toStdString();
	// �Ƚ�������������մ���...
	m_strUTF8Data.clear();
	// �ж���ϣ�����ȡ�����ƽ��Һ��뷢�͵���������֤...
	char  szUrl[MAX_PATH] = {0};
	char  szPost[MAX_PATH] = { 0 };
	sprintf(szUrl, "%s/wxapi.php/Gather/loginLiveRoom", App()->GetWebAddr().c_str());
	sprintf(szPost, "room_id=%s&type_id=%d", strLiveRoomID.c_str(), App()->GetClientType());
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
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Empty"));
		ui->errLabel->setText(QTStr("Teacher.Room.Empty"));
		return;
	}
	// ��ʼ�������ݹ����� json ���ݰ�...
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
	// ����������ȡ���Ĵ洢��������ַ�Ͷ˿�...
	json_t * theTrackerAddr = json_object_get(theRoot, "tracker_addr");
	json_t * theTrackerPort = json_object_get(theRoot, "tracker_port");
	if (theTrackerAddr == NULL || theTrackerPort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ����Զ����ת��������ַ�Ͷ˿�...
	json_t * theRemoteAddr = json_object_get(theRoot, "remote_addr");
	json_t * theRemotePort = json_object_get(theRoot, "remote_port");
	if (theRemoteAddr == NULL || theRemotePort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ����udp��������ַ�Ͷ˿�...
	json_t * theUdpAddr = json_object_get(theRoot, "udp_addr");
	json_t * theUdpPort = json_object_get(theRoot, "udp_port");
	if (theUdpAddr == NULL || theUdpPort == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ����������ȡ���ķ�����Ľ�ʦ��ѧ������...
	json_t * theTeacherCount = json_object_get(theRoot, "teacher");
	json_t * theStudentCount = json_object_get(theRoot, "student");
	if (theTeacherCount == NULL || theStudentCount == NULL) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Json"));
		ui->errLabel->setText(QTStr("Teacher.Room.Json"));
		json_decref(theRoot);
		return;
	}
	// ���㲢�жϷ�����Ľ�ʦ����������0�����ܵ�¼...
	int nTeacherCount = atoi(json_string_value(theTeacherCount));
	int nStudentCount = atoi(json_string_value(theStudentCount));
	if (nTeacherCount > 0) {
		//OBSMessageBox::information(this, QTStr("Teacher.Error.Title"), QTStr("Teacher.Room.Login"));
		ui->errLabel->setText(QTStr("Teacher.Room.Login"));
		json_decref(theRoot);
		return;
	}
	// û�д������ش����...
	ui->errLabel->hide();
	// ��ȡ��ֱ���ķֽ����ݣ�����ֱ����ַ���浽 => obs-teacher/global.ini...
	config_set_string(App()->GlobalConfig(), "General", "LiveRoomID", strLiveRoomID.c_str());
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
	// ֪ͨ�����������ڿ��������ˣ����ݷ����...
	emit this->loginSuccess(strLiveRoomID);
}
//
// ��Ӧ������󱻵���¼�...
void LoginWindow::onClickRoomItem(int nDBRoomID)
{
	int nLiveRoomID = m_nBeginID + nDBRoomID;
	this->doLoginAction(nLiveRoomID);
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
	painter.fillPath(pathBack, QBrush(QPixmap(":/login/images/login/back.jpg").scaled(this->size())));
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

// �����¼��ƶ�������ʾλ��...
void LoginWindow::wheelEvent(QWheelEvent *event)
{
	// �����ĽǶȣ�*8�����������ľ���...
	// �����Ĳ�����*15�����������ĽǶ�...
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;
	// ֻ����ֱ�����¼�...
	if (event->orientation() != Qt::Vertical)
		return;
	// �����ǰҳ�����������������Ч�ģ�ֱ�ӷ���...
	if (m_nMaxPage <= 0 || m_nCurPage <= 0 || m_nTotalNum <= 0)
		return;
	// ���㷭ҳ��������Ҫ����ҳԽ������...
	int nCalcPage = m_nCurPage + numSteps;
	nCalcPage = (nCalcPage <= 1) ? 1 : nCalcPage;
	nCalcPage = (nCalcPage >= m_nMaxPage) ? m_nMaxPage : nCalcPage;
	// ���������ҳ���뵱ǰҳһ�£�û�б仯��ֱ�ӷ���...
	if (nCalcPage == m_nCurPage) return;
	// ���ýӿڽ����µ�һҳ�����б��ȡ���ڲ����������...
	emit this->doTriggerRoomList(nCalcPage);
}

void LoginWindow::closeEvent(QCloseEvent *event)
{
	return __super::closeEvent(event);
}
