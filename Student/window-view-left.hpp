
#pragma once

#include <map>
#include <functional>
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
private:
	void LayoutViewCamera(QSize & inSize);
protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
private:
	int					m_nFirstID;			// ��һ������ͷ���ڵı��...
	GM_MapCamera		m_MapCamera;		// ����ͷ���ڼ��� => DBCameraID => CViewCamera
};
