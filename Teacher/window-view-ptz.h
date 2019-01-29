
#pragma once

#include "HYDefine.h"
#include "ui_PTZWindow.h"

#include <string>
#include <functional>

using namespace std;

class CPTZWindow : public QDialog
{
    Q_OBJECT
public:
	CPTZWindow(QWidget *parent);
    ~CPTZWindow();
public:
	void doUpdatePTZ(int nDBCameraID);
protected:
	void loadStyleSheet(const QString &sheetName);
private:
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void onTriggerCameraPTZ(int nDBCameraID);
	void onButtonCloseClicked();
	void onSliderChanged(int value);
	void onFlipIndexChanged(int index);
	void onBtnUpPressed();
	void onBtnDownPressed();
	void onTiltFinish();
	void onBtnLeftPressed();
	void onBtnRightPressed();
	void onPanFinish();
	void onAddZoomPressed();
	void onSubZoomPressed();
	void onZoomFinish();
	void onAddFocusPressed();
	void onSubFocusPressed();
	void onFocusFinish();
	void onAddIrisPressed();
	void onSubIrisPressed();
	void onIrisFinish();
private:
	void initWindow();
	void doUpdateImageFlip(string & inFlipVal);
	bool doPTZCmd(CMD_ISAPI inCMD, int inSpeedVal);
private:
	Ui::PTZWindow  *  ui;
	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
	int               m_nDBCameraID = 0;
	int               m_nSpeedValue = 0;
};
