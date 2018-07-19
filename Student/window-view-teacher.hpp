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
	QString		m_strTitleBase;		// ��������������...
	QString		m_strTitleText;		// ��������������...
	QRect		m_rcNoramlRect;		// ���ڵ�ȫ��ǰ�ľ�������...
	QRect		m_rcRenderRect;		// ���ڻ�����Ⱦ�ľ�������...
};
