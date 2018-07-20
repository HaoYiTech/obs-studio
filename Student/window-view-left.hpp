
#pragma once

#include <map>
#include <functional>
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
private:
	void LayoutViewCamera(QSize & inSize);
protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
private:
	int					m_nFirstID;			// 第一个摄像头窗口的编号...
	GM_MapCamera		m_MapCamera;		// 摄像头窗口集合 => DBCameraID => CViewCamera
};
