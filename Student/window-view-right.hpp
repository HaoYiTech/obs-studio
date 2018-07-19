#pragma once

#include "qt-display.hpp"

class CViewTeacher;
class CViewRight : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRight(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRight();
public:
	void onFullScreenAction();
protected:
	void resizeEvent(QResizeEvent *event) override;
private:
	CViewTeacher   *   m_lpViewTeacher;
};
