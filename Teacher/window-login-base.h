
#pragma once

#include <QDialog>
#include "window-login-title.h"

class BaseWindow : public QDialog
{
	Q_OBJECT
public:
	BaseWindow(QWidget *parent = NULL);
	~BaseWindow();
public:
	void setBackGroundColor(QColor inColor) { m_bkColor = inColor; }
protected:
	void initTitleBar();
	void paintEvent(QPaintEvent *event);
	void loadStyleSheet(const QString &sheetName);
private slots:
	void onButtonMinClicked();
	void onButtonCloseClicked();
protected:
	MyTitleBar * m_titleBar;
	QColor       m_bkColor;
};
