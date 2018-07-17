
#include "window-view-left.hpp"

CViewLeft::CViewLeft(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	m_bkColor = QColor(64, 64, 64);
}

CViewLeft::~CViewLeft()
{
}
