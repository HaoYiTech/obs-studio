
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

using namespace std;

class CCenterSession;
class CLoginMini : public QDialog
{
    Q_OBJECT
public:
	CLoginMini(QWidget *parent = NULL);
    ~CLoginMini();
signals:
	void doTriggerMiniSuccess();
protected:
	void loadStyleSheet(const QString &sheetName);
	void closeEvent(QCloseEvent *event) override;
private:
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void ToggleShowHide();
	void onButtonMinClicked();
	void onButtonCloseClicked();
	void SetShowing(bool showing);
	void IconActivated(QSystemTrayIcon::ActivationReason reason);
private:
	void initWindow();
	void SystemTrayInit();
	void SystemTray(bool firstStarted);
	void timerEvent(QTimerEvent * inEvent);
	bool parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin);
	void CheckForUpdates(bool manualUpdate);
	void TimedCheckForUpdates();

	void EnumDialogs();
	QList<QDialog*> visDialogs;
	QList<QDialog*> modalDialogs;
	QList<QMessageBox*> visMsgBoxes;
	QList<QPoint> visDlgPositions;
private:
	Ui::LoginMini      *      ui;
	QPointer<QSystemTrayIcon> trayIcon;
	QPointer<QAction>         showHide;
	QPointer<QAction>         exit;
	QPointer<QMenu>           trayMenu;

	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
	QString           m_strVer;                 // 显示版本信息内容文字...
	//QPointer<QThread> updateCheckThread = NULL; // 升级检测线程...
};
