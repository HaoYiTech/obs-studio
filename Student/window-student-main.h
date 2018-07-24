#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_StudentWindow.h"

class StudentWindow : public QMainWindow
{
	Q_OBJECT
public:
	StudentWindow(QWidget *parent = Q_NULLPTR);
	virtual ~StudentWindow();
public:
	void InitWindow();
	void UpdateTitleBar();
	CViewLeft * GetViewLeft() { return m_ui.LeftView; }
protected:
	void closeEvent(QCloseEvent *event) override;
private slots:
	void doWebThreadMsg(int nMessageID, int nWParam, int nLParam);
	void on_actionSystemFullscreen_triggered();
	void on_actionSystemToolbar_triggered();
	void on_actionSystemExit_triggered();
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
	Ui::StudentClass m_ui;
};
