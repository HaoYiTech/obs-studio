#pragma once

#include "qt-display.hpp"

class CViewTeacher;
class CViewRight : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRight(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRight();
public:
	void doDestroyResource();
	void onFullScreenAction();
	void onWebLoadResource();
	void onWebAuthResult(int nType, bool bAuthOK);
protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
private:
	CViewTeacher   *   m_lpViewTeacher;
	QString            m_strNotice;
};
