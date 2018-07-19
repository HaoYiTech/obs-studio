
#pragma once

#include "qt-display.hpp"

class CViewLeft : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewLeft(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewLeft();
protected:
	void resizeEvent(QResizeEvent *event) override;
};
