
#pragma once

#include "qt-display.hpp"
#include "HYDefine.h"

using namespace std;

class CViewCamera;
// ע�⣺���������ͷ�����ǰ��ս�������...
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
	void onCameraDel(int nDBCameraID);	// ɾ��ͨ��...
	void doDestoryResource();			// ɾ����Դ...
	void onWebLoadResource();			// ��¼�ɹ�...
	void onWebAuthExpired();			// ��Ȩ����...
	void onCameraStart();				// ����ͨ��...
	void onCameraStop();				// ֹͣͨ��...
	void onPagePrev();					// �����һҳ...
	void onPageJump();					// �����תҳ...
	void onPageNext();					// �����һҳ...
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
	int             m_nAutoTimer;		// IPC�Զ�����ʱ��...
	int				m_nCurPage;			// ��ǰҳ...
	int				m_nMaxPage;			// ��ҳ��...
	int				m_nFirstID;			// ��һ������ͷ���ڵı��...
	int				m_nFocusID;			// ��ǰ��Ч��������ͷ���ڵı��...
	int             m_nCurAutoID;       // ��ǰ��Ҫ����������ͷ���ݿ���...
	bool            m_bCanAutoLink;		// �ܷ��Զ�������־ => �޸�ͨ��ʱ�����Զ�����...
	GM_MapCamera	m_MapCamera;		// ����ͷ���ڼ��� => DBCameraID => CViewCamera
};
