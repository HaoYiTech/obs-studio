
#include "window-view-camera.hpp"

CViewCamera::CViewCamera(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	m_bkColor = QColor(64, 64, 64);
}

CViewCamera::~CViewCamera()
{
}
