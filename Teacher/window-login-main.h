
#pragma once

#include "window-login-base.h"
#include "ui_LoginWindow.h"
#include <string>

using namespace std;

class LoginWindow : public BaseWindow
{
    Q_OBJECT
public:
    LoginWindow(QWidget *parent = NULL);
    ~LoginWindow();
signals:
	void loginSuccess(string & strRoomID);
public:
	void doPostCurl(char *pData, size_t nSize);
private:
    void initMyTitle() ;
	void initWindow();

	void paintEvent(QPaintEvent *event);
	void closeEvent(QCloseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void onClickLoginButton();
	void onClickArrowButton();
private:
	Ui::LoginWindow * ui;
	string	m_strUTF8Data;
	QPoint	m_startMovePos;
	bool	m_isPressed;
};
