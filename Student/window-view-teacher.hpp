#pragma once

#include <qevent.h>
#include <QMouseEvent>
#include "qt-display.hpp"

class CUDPRecvThread;
class CViewRender;
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
protected slots:
	void onBuildUDPRecvThread(bool bIsUDPTeacherOnLine);
protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
private:
	CUDPRecvThread  *  m_lpUDPRecvThread;   // UDP接收线程...
	CViewRender     *  m_lpViewRender;      // 渲染显示窗口...
	QString            m_strTitleBase;      // 标题栏基础文字...
	QString            m_strTitleText;      // 标题栏文字内容...
};
