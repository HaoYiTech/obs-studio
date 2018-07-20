#pragma once

#include <QMouseEvent>
#include "qt-display.hpp"

class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewCamera();
public:
	void setTitleContent(QString & titleContent);
private:
	void DrawTitleArea();
	void DrawRenderArea();
protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
private:
	QString		m_strTitleText;		// 标题栏文字内容...
	QRect		m_rcRenderRect;		// 窗口画面渲染的矩形区域...
};
