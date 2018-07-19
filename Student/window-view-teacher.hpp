#pragma once

#include <qevent.h>
#include <QMouseEvent>
#include "qt-display.hpp"

class CViewTeacher : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewTeacher(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewTeacher();
public:
	void setTitleContent(QString & titleContent);
	void onFullScreenAction();
private:
	void DrawTitleArea();
	void DrawRenderArea();
protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
	QString		m_strTitleBase;		// 标题栏基础文字...
	QString		m_strTitleText;		// 标题栏文字内容...
	QRect		m_rcNoramlRect;		// 窗口的全屏前的矩形区域...
	QRect		m_rcRenderRect;		// 窗口画面渲染的矩形区域...
};
