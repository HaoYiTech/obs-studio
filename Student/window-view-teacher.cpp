
#include "window-view-teacher.hpp"

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	m_bkColor = QColor(200, 32, 32);
}

CViewTeacher::~CViewTeacher()
{
}

void CViewTeacher::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewTeacher::onFullScreenAction()
{
	if( this->isFullScreen() ) {
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
	} else {
		this->setWindowFlags(Qt::Window);
		this->showFullScreen();
	}
}

void CViewTeacher::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
	}
}