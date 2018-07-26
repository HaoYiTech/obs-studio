
#include <qevent.h>
#include <QScreen>
#include <QPainter>
#include <QGuiApplication>

#include "HYDefine.h"
#include "student-app.h"
#include "window-view-right.hpp"
#include "window-view-teacher.hpp"

#define NOTICE_FONT_HEIGHT		30
#define NOTICE_TEXT_COLOR		QColor(255,255,0)

CViewRight::CViewRight(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_lpViewTeacher(NULL)
{
	// 设置默认的窗口填充背景色...
	m_bkColor = QColor(32, 32, 32);
	// 设置默认的提示信息...
	m_strNotice = QTStr("Left.Window.DefaultNotice");
	// 设置窗口字体大小...
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(NOTICE_FONT_HEIGHT);
	theFont.setWeight(QFont::Light);
	this->setFont(theFont);

	//m_lpViewTeacher = new CViewTeacher(this);
	//m_lpViewTeacher->setTitleContent(QStringLiteral("码流：1024kbps"));
}

CViewRight::~CViewRight()
{
	// 这里不删除的话QT也会自动删除...
	this->doDestroyResource();
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

void CViewRight::onWebAuthResult(int nType, bool bAuthOK)
{
	// 根据不同状态，显示不同的信息...
	if (nType == kAuthRegister) {
		m_strNotice = bAuthOK ? QTStr("Right.Window.WaitRegister") : QTStr("Right.Window.ErrRegister");
	} else if (nType == kAuthExpired) {
		m_strNotice = bAuthOK ? QTStr("Right.Window.WaitCamera") : QTStr("Right.Window.Expired");
	}
	// 重新更新窗口背景...
	this->update();
}

// 网络线程注册学生端成功之后的操作...
void CViewRight::onWebLoadResource()
{
	if (m_lpViewTeacher != NULL)
		return;
	QRect & rcRect = this->rect();
	ASSERT(m_lpViewTeacher == NULL);
	m_lpViewTeacher = new CViewTeacher(this);
	m_lpViewTeacher->resize(rcRect.width(), rcRect.height());
	m_lpViewTeacher->show();
}

void CViewRight::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
	// 如果老师窗口已经创建，直接返回...
	if (m_lpViewTeacher != NULL)
		return;
	// 设置信息栏文字颜色...
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	// 自动居中显示文字...
	painter.drawText(this->rect(), Qt::AlignCenter, m_strNotice);
}

void CViewRight::doDestroyResource()
{
	// 删除讲师端窗口对象...
	if (m_lpViewTeacher != NULL) {
		delete m_lpViewTeacher;
		m_lpViewTeacher = NULL;
	}
	// 设置默认的提示信息...
	m_strNotice = QTStr("Left.Window.DefaultNotice");
	// 更新窗口界面信息...
	this->update();
}