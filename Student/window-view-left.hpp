
#pragma once

#include "qt-display.hpp"
#include "HYDefine.h"

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
	void onTriggerConnected();
	void doEnableCamera(OBSQTDisplay * lpNewDisplay);
	void onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID);
public:
	CViewCamera * FindDBCameraByID(int nDBCameraID);
	CViewCamera * AddNewCamera(GM_MapData & inWebData);
public:
	void SetCanAutoLink(bool bIsCanAuto) { m_bCanAutoLink = bIsCanAuto; }
	int  GetCurPage() { return m_nCurPage; }
	int  GetMaxPage() { return m_nMaxPage; }
	int  GetFocusID() { return m_nFocusID; }
	void onCameraDel(int nDBCameraID);	// 删除通道...
	void doDestoryResource();			// 删除资源...
	void onWebLoadResource();			// 登录成功...
	void onWebAuthExpired();			// 授权过期...
	void onCameraStart();				// 启动通道...
	void onCameraStop();				// 停止通道...
	void onPagePrev();					// 点击上一页...
	void onPageJump();					// 点击跳转页...
	void onPageNext();					// 点击下一页...
private:
	void doAutoLinkIPC();
	void LayoutViewCamera(int cx, int cy);
	int  GetNextAutoID(int nCurDBCameraID);
	void doStopCurUdpSendThread(int nNewDBCameraID);
	CViewCamera * BuildWebCamera(GM_MapData & inWebData);
protected:
	void wheelEvent(QWheelEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void timerEvent(QTimerEvent * inEvent) override;
private:
	int             m_nAutoTimer;		// IPC自动重连时钟...
	int				m_nCurPage;			// 当前页...
	int				m_nMaxPage;			// 总页数...
	int				m_nFirstID;			// 第一个摄像头窗口的编号...
	int				m_nFocusID;			// 当前有效焦点摄像头窗口的编号...
	int             m_nCurAutoID;       // 当前需要重连的摄像头数据库编号...
	bool            m_bCanAutoLink;		// 能否自动重连标志 => 修改通道时不能自动重连...
	GM_MapCamera	m_MapCamera;		// 摄像头窗口集合 => DBCameraID => CViewCamera
};
