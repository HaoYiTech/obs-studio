
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
		kMiniLoginRoom  = 0,  // ��ʽ��¼ָ���������...
		kMiniRemoteConn = 1,  // ���ӷ�������TCP��ַ�Ͷ˿�...
	} m_eMiniState;
private:
	Ui::LoginMini      *      ui;
	QPointer<QSystemTrayIcon> trayIcon;
	QPointer<QAction>         showHide;
	QPointer<QAction>         exit;
	QPointer<QMenu>           trayMenu;

	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
	QMovie     *      m_lpMovieGif = nullptr;   // GIF��������...
	QLabel     *      m_lpLoadBack = nullptr;   // GIF��������...
	int               m_nOnLineTimer = -1;      // ���������߼��ʱ��...
	int               m_nFailedTimer = -1;      // ��¼ʧ�ܼ��ʱ��...
	QString           m_strScan;                // ��ʾ״̬��Ϣ�ַ�������...
	QString           m_strVer;                 // ��ʾ�汾��Ϣ��������...
	QString           m_strUser;                // ѧ������...
	QString           m_strRoom;                // �������...
	QString           m_strPass;                // ��������...
	QPointer<CRemoteSession> m_RemoteSession;   // Զ������...
	QNetworkAccessManager    m_objNetManager;	// QT ����������...
	//QPointer<QThread> updateCheckThread = NULL; // ��������߳�...
};
