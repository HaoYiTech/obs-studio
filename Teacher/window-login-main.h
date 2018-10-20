
#pragma once

#include <QNetworkAccessManager>
#include "window-login-base.h"
#include "ui_LoginWindow.h"
#include <string>

using namespace std;

class LoginWindow;
class QNetworkReply;
class CRoomItem : public QLabel
{
	Q_OBJECT
public:
	CRoomItem(int nRoomIndex, LoginWindow * lpWindow);
	~CRoomItem();
public:
	void doDisplayItem(QPixmap & inPixmap);
private:
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void enterEvent(QEvent *event);
	virtual void leaveEvent(QEvent *event);
private:
	enum {
		kBorderSize   = 2,
		kColSize      = 4,
		kRadiusSize   = 8,
		kTextFontSize = 10,
		kRoomFontSize = 20,
		kMarginSpace  = 10,
		kTitleSpace   = 30,
		kMaskHeight   = 100,
		kPosterWidth  = 200,
		kPosterHeight = 300,
	};
	QNetworkReply * m_lpNetReply;
	LoginWindow   * m_lpWindow;
	QString         m_strEndTime;
	QString         m_strStartTime;
	QString         m_strTeacherName;
	QString         m_strPosterUrl;
	QString         m_strRoomName;
	int             m_nRoomIndex;
	int             m_nDBRoomID;
	bool            m_bIsHover;

	friend class LoginWindow;
};

class LoginWindow : public BaseWindow
{
    Q_OBJECT
public:
    LoginWindow(QWidget *parent = NULL);
    ~LoginWindow();
signals:
	void loginSuccess(string & strRoomID);
	void doTriggerRoomList(int nRoomPage);
public:
	int  GetRoomBeginID() { return m_nBeginID; }
	void doPostCurl(char *pData, size_t nSize);
	void onClickRoomItem(int nDBRoomID);
private:
	void initLoadGif();
	void initMyTitle();
	void initWindow();

	void initArrayRoom();
	void hideArrayRoom();
	void clearArrayRoom();

	void doUpdateTitle();
	void doLoginAction(int nLiveRoomID);

	virtual void paintEvent(QPaintEvent *event);
	virtual void closeEvent(QCloseEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);
private slots:
	void onClickLoginButton();
	void onClickArrowButton();
	void onTriggerRoomList(int nRoomPage);
	void replyFinished(QNetworkReply *reply);
private:
	enum { kPageRoom = 8 };
private:
	Ui::LoginWindow * ui;
	bool	          m_isPressed;
	string	          m_strUTF8Data;
	QPoint	          m_startMovePos;
	QMovie     *      m_lpMovieGif;
	QLabel     *      m_lpLoadBack;
	int               m_nTotalNum;
	int               m_nMaxPage;
	int               m_nCurPage;
	int               m_nBeginID;
	CRoomItem  *      m_ArrayRoom[kPageRoom]; // 注意：用数组的原因是重建Label无法显示，只能用显示和隐藏的方式解决...
	QNetworkAccessManager  m_objNetManager;   // 网络访问对象，用于海报图片的网络显示管理工作...
};
