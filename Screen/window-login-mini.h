
#pragma once

#include "HYCommon.h"
#include "ui_LoginMini.h"
#include "json.h"

#include <string>
#include <functional>
#include <QMenu>
#include <QThread>
#include <QPointer>
#include <QAction>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QNetworkAccessManager>

using namespace std;

class CRemoteSession;
class CLoginMini : public QDialog
{
    Q_OBJECT
public:
	CLoginMini(QWidget *parent = NULL);
    ~CLoginMini();
public:
	void doSuccessOBS();
	bool doSendScreenSnap(uint8_t * inData, int nSize);
signals:
	void doTriggerMiniSuccess();
protected:
	void loadStyleSheet(const QString &sheetName);
	void closeEvent(QCloseEvent *event) override;
private:
	virtual void reject();
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void ToggleShowHide();
	void onTriggerConnected();
	void onClickLoginButton();
	void onClickLogoutButton();
	void onButtonMinClicked();
	void onButtonCloseClicked();
	void SetShowing(bool showing);
	void onReplyFinished(QNetworkReply *reply);
	void IconActivated(QSystemTrayIcon::ActivationReason reason);
private:
	void initWindow();
	bool doCheckOnLine();
	void doLoginFailed();
	void SystemTrayInit();
	void SystemTray(bool firstStarted);
	void timerEvent(QTimerEvent * inEvent);
	void doTcpConnRemote();
	void doWebGetMiniLoginRoom();
	void onProcMiniLoginRoom(QNetworkReply *reply);
	bool parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin);
	void CheckForUpdates(bool manualUpdate);
	void TimedCheckForUpdates();

	void EnumDialogs();
	QList<QDialog*> visDialogs;
	QList<QDialog*> modalDialogs;
	QList<QMessageBox*> visMsgBoxes;
	QList<QPoint> visDlgPositions;
private:
	enum {
		kMiniLoginRoom  = 0,  // 正式登录指定房间号码...
		kMiniRemoteConn = 1,  // 连接服务器的TCP地址和端口...
	} m_eMiniState;
private:
	Ui::LoginMini      *      ui;
	QPointer<QSystemTrayIcon> trayIcon;
	QPointer<QAction>         showHide;
	QPointer<QAction>         exit;
	QPointer<QMenu>           trayMenu;

	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
	QMovie     *      m_lpMovieGif = nullptr;   // GIF动画对象...
	QLabel     *      m_lpLoadBack = nullptr;   // GIF动画背景...
	int               m_nOnLineTimer = -1;      // 服务器在线检测时钟...
	int               m_nFailedTimer = -1;      // 登录失败检测时钟...
	QString           m_strScan;                // 提示状态信息字符串内容...
	QString           m_strVer;                 // 显示版本信息内容文字...
	QString           m_strUser;                // 学生姓名...
	QString           m_strRoom;                // 房间号码...
	QString           m_strPass;                // 房间密码...
	QPointer<CRemoteSession> m_RemoteSession;   // 远程连接...
	QNetworkAccessManager    m_objNetManager;	// QT 网络管理对象...
	//QPointer<QThread> updateCheckThread = NULL; // 升级检测线程...
};
