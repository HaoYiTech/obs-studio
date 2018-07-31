
#pragma once

#include "HYDefine.h"
#include "qt-display.hpp"

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
	void doEnableCamera(OBSQTDisplay * lpNewDisplay);
public:
	CViewCamera * FindDBCameraByID(int nDBCameraID);
	CViewCamera * AddNewCamera(GM_MapData & inWebData);
public:
	int  GetCurPage() { return m_nCurPage; }
	int  GetMaxPage() { return m_nMaxPage; }
	int  GetFocusID() { return m_nFocusID; }
	void onCameraDel(int nDBCameraID);	// ɾ��ͨ��...
	void onTriggerUdpSendThread();      // �¼�֪ͨ...
	void doDestoryResource();			// ɾ����Դ...
	void onWebLoadResource();			// ��¼�ɹ�...
	void onWebAuthExpired();			// ��Ȩ����...
	void onCameraStart();				// ����ͨ��...
	void onCameraStop();				// ֹͣͨ��...
	void onPagePrev();					// �����һҳ...
	void onPageJump();					// �����תҳ...
	void onPageNext();					// �����һҳ...
private:
	void LayoutViewCamera(int cx, int cy);
	CViewCamera * BuildWebCamera(GM_MapData & inWebData);
protected:
	void wheelEvent(QWheelEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
private:
	int				m_nCurPage;			// ��ǰҳ...
	int				m_nMaxPage;			// ��ҳ��...
	int				m_nFirstID;			// ��һ������ͷ���ڵı��...
	int				m_nFocusID;			// ��ǰ��Ч��������ͷ���ڵı��...
	GM_MapCamera	m_MapCamera;		// ����ͷ���ڼ��� => DBCameraID => CViewCamera
};
