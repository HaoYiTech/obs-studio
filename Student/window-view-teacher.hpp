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
	CUDPRecvThread  *  m_lpUDPRecvThread;   // UDP�����߳�...
	CViewRender     *  m_lpViewRender;      // ��Ⱦ��ʾ����...
	QString            m_strTitleBase;      // ��������������...
	QString            m_strTitleText;      // ��������������...
};
