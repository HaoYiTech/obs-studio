#pragma once

#include "qt-display.hpp"

class CViewRight : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRight(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRight();
};
