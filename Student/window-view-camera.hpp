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
	bool IsCameraPreview() { return m_bIsPreview; }
	int  GetDBCameraID() { return m_nDBCameraID; }
	void onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
	void doTogglePreview();
	bool doCameraStart();
	bool doCameraStop();
	void doStatReport();
private:
	bool IsLiveState();
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
	int               m_nDBCameraID;      // 通道在数据库中的编号...
	bool              m_bIsPreview;       // 通道是否正在预览画面...
	QRect             m_rcRenderRect;     // 窗口画面渲染的矩形区域...
	CViewLeft      *  m_lpViewLeft;       // 左侧父窗口对象...
	CPushThread    *  m_lpPushThread;     // 推流数据管理器...
};
