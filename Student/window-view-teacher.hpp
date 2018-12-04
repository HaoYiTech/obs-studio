#pragma once

#include <qevent.h>
#include <QMouseEvent>
#include "qt-display.hpp"

class CUDPMultiRecvThread;
class CUDPRecvThread;
class CViewRender;
class CViewTeacher : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewTeacher(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewTeacher();
public:
	void  setTitleContent(QString & titleContent);
	bool  doVolumeEvent(int inKeyItem);
	void  onFullScreenAction();
	void  doResetMulticastIPSend();
private:
	void  DrawTitleArea();
	void  DrawRenderArea();
	bool  doRoleMultiRecv();
	bool  doRoleMultiSend();
public slots:
	void  onTriggerUdpRecvThread(bool bIsUDPTeacherOnLine);
protected:
	void  paintEvent(QPaintEvent *event) override;
	void  mousePressEvent(QMouseEvent *event) override;
private:
	CUDPMultiRecvThread  *  m_lpUDPMultiRecvThread; // UDP�鲥�����߳�...
	CUDPRecvThread       *  m_lpUDPRecvThread;		// UDP�����߳�...
	CViewRender          *  m_lpViewRender;			// ��Ⱦ��ʾ����...
	QString                 m_strTitleBase;			// ��������������...
	QString                 m_strTitleText;			// ��������������...
};
