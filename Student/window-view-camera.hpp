#pragma once

#include <QMouseEvent>
#include "qt-display.hpp"

class CViewLeft;
class CPushThread;
class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, int nDBCameraID);
	virtual ~CViewCamera();
public:
	bool IsCameraLogin() { return ((m_lpPushThread != NULL) ? true : false); }
	int  GetDBCameraID() { return m_nDBCameraID; }
	void onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
	bool doCameraStart();
	bool doCameraStop();
private:
	void DrawTitleArea();
	void DrawRenderArea();
	void DrawStatusText();
	QString GetRecvPullRate();
	QString GetSendPushRate();
	QString GetStreamPushUrl();
protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
private:
	int               m_nDBCameraID;      // ͨ�������ݿ��еı��...
	QRect             m_rcRenderRect;     // ���ڻ�����Ⱦ�ľ�������...
	CViewLeft      *  m_lpViewLeft;       // ��ุ���ڶ���...
	CPushThread    *  m_lpPushThread;     // �������ݹ�����...
};
