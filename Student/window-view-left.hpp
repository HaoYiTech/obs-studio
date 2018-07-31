
#pragma once

#include "HYDefine.h"
#include "qt-display.hpp"

using namespace std;

class CViewCamera;
// 注意：这里的摄像头窗口是按照降序排列...
typedef	map<int, CViewCamera*, greater<int>> GM_MapCamera;

class CViewLeft : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewLeft(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewLeft();
signals:
	void enablePagePrev(bool bEnable);
	void enablePageJump(bool bEnable);
	void enablePageNext(bool bEnable);
	void enableCameraAdd(bool bEnable);
	void enableCameraMod(bool bEnable);
	void enableCameraDel(bool bEnable);
	void enableCameraStop(bool bEnable);
	void enableCameraStart(bool bEnable);
	void enableSettingSystem(bool bEnable);
	void enableSettingReconnect(bool bEnable);
	void enableSystemFullscreen(bool bEnable);
public slots:
	void doEnableCamera(OBSQTDisplay * lpNewDisplay);
public:
	CViewCamera * FindDBCameraByID(int nDBCameraID);
	CViewCamera * AddNewCamera(GM_MapData & inWebData);
public:
	int  GetCurPage() { return m_nCurPage; }
	int  GetMaxPage() { return m_nMaxPage; }
	int  GetFocusID() { return m_nFocusID; }
	void onCameraDel(int nDBCameraID);	// 删除通道...
	void onTriggerUdpSendThread();      // 事件通知...
	void doDestoryResource();			// 删除资源...
	void onWebLoadResource();			// 登录成功...
	void onWebAuthExpired();			// 授权过期...
	void onCameraStart();				// 启动通道...
	void onCameraStop();				// 停止通道...
	void onPagePrev();					// 点击上一页...
	void onPageJump();					// 点击跳转页...
	void onPageNext();					// 点击下一页...
private:
	void LayoutViewCamera(int cx, int cy);
	CViewCamera * BuildWebCamera(GM_MapData & inWebData);
protected:
	void wheelEvent(QWheelEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
private:
	int				m_nCurPage;			// 当前页...
	int				m_nMaxPage;			// 总页数...
	int				m_nFirstID;			// 第一个摄像头窗口的编号...
	int				m_nFocusID;			// 当前有效焦点摄像头窗口的编号...
	GM_MapCamera	m_MapCamera;		// 摄像头窗口集合 => DBCameraID => CViewCamera
};
