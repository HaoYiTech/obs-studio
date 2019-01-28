#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_StudentWindow.h"

#include <QPointer>
#include <QThread>

class CPTZWindow;
class StudentWindow : public QMainWindow
{
	Q_OBJECT
public:
	StudentWindow(QWidget *parent = Q_NULLPTR);
	virtual ~StudentWindow();
public:
	void InitWindow();
	void UpdateTitleBar();
	void doUpdatePTZ(int nDBCameraID);
	bool VolumeEvent(QObject * inObject, int inKeyItem);
	CViewLeft  * GetViewLeft() { return m_ui.LeftView; }
	CViewRight * GetViewRight() { return m_ui.RightView; }
	void SetSlientClose(bool bIsSlient) { m_bIsSlientClose = bIsSlient; }
protected:
	void closeEvent(QCloseEvent *event) override;
private slots:
	void updateCheckFinished();
    void onCameraMenuToShow();
	void doWebThreadMsg(int nMessageID, int nWParam, int nLParam);
	void onTriggerUdpLogout(int tmTag, int idTag, int nDBCameraID);
	void on_LeftViewCustomContextMenuRequested(const QPoint &pos);
	void on_actionSettingReconnect_triggered();
	void on_actionSettingUpdater_triggered();
	void on_actionSettingSystem_triggered();
	void on_actionSystemFullscreen_triggered();
	void on_actionSystemToolbar_triggered();
	void on_actionSystemExit_triggered();
	void on_actionCameraPreview_triggered();
	void on_actionPreviewMute_triggered();
	void on_actionCameraPTZ_triggered();
	void on_actionCameraAdd_triggered();
	void on_actionCameraMod_triggered();
	void on_actionCameraDel_triggered();
	void on_actionCameraStart_triggered();
	void on_actionCameraStop_triggered();
	void on_actionPagePrev_triggered();
	void on_actionPageJump_triggered();
	void on_actionPageNext_triggered();
	void on_actionHelpAbout_triggered();
private:
	void TimedCheckForUpdates();
	void CheckForUpdates(bool manualUpdate);
private:
	Ui::StudentClass m_ui;
	bool m_bIsSlientClose = false;
	QPointer<QThread> updateCheckThread;
	QPointer<CPTZWindow> m_PTZWindow = NULL;
};
