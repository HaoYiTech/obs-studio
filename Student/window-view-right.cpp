
#include <qevent.h>
#include <QScreen>
#include <QGuiApplication>

#include "window-view-right.hpp"
#include "window-view-teacher.hpp"

CViewRight::CViewRight(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_lpViewTeacher(NULL)
{
	m_bkColor = QColor(32, 32, 32);
	m_lpViewTeacher = new CViewTeacher(this);
	//m_lpViewTeacher->setTitleContent(QStringLiteral("码流：1024kbps"));
}

CViewRight::~CViewRight()
{
	// 这里不删除的话QT也会自动删除...
	if( m_lpViewTeacher != NULL ) {
		delete m_lpViewTeacher;
		m_lpViewTeacher = NULL;
	}
}

void CViewRight::resizeEvent(QResizeEvent *event)
{
	if (m_lpViewTeacher != NULL) {
		m_lpViewTeacher->resize(event->size());
	}
	QWidget::resizeEvent(event);
}

void CViewRight::onFullScreenAction()
{
	if (m_lpViewTeacher == NULL)
		return;
	m_lpViewTeacher->onFullScreenAction();
}