
#include "window-view-teacher.hpp"

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	m_bkColor = QColor(32, 32, 32);
}

CViewTeacher::~CViewTeacher()
{
}
