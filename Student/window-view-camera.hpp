#pragma once

#include "qt-display.hpp"

class CViewCamera : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewCamera(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewCamera();
};
