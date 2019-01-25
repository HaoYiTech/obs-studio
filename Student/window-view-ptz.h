
#pragma once

#include "ui_PTZWindow.h"

class CPTZWindow : public QDialog
{
    Q_OBJECT
public:
	CPTZWindow(QWidget *parent = NULL);
    ~CPTZWindow();
private:
	void initMyTitle();
	void initWindow();
protected:
	void loadStyleSheet(const QString &sheetName);
private:
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void onButtonCloseClicked();
	void onSliderChanged(int value);
private:
	Ui::PTZWindow  *  ui;
	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
};
