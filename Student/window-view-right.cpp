
#include "window-view-right.hpp"

CViewRight::CViewRight(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	m_bkColor = QColor(32, 32, 32);
}

CViewRight::~CViewRight()
{
}
