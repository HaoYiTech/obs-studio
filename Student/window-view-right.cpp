
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
	// ����Ĭ�ϵĴ�����䱳��ɫ...
	m_bkColor = QColor(32, 32, 32);
	// ����Ĭ�ϵ���ʾ��Ϣ...
	m_strNotice = QTStr("Left.Window.DefaultNotice");
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(NOTICE_FONT_HEIGHT);
	theFont.setWeight(QFont::Light);
	this->setFont(theFont);

	//m_lpViewTeacher = new CViewTeacher(this);
	//m_lpViewTeacher->setTitleContent(QStringLiteral("������1024kbps"));
}

CViewRight::~CViewRight()
{
	// ���ﲻɾ���Ļ�QTҲ���Զ�ɾ��...
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
	// ���ݲ�ͬ״̬����ʾ��ͬ����Ϣ...
	if (nType == kAuthRegister) {
		m_strNotice = bAuthOK ? QTStr("Right.Window.WaitRegister") : QTStr("Right.Window.ErrRegister");
	} else if (nType == kAuthExpired) {
		m_strNotice = bAuthOK ? QTStr("Right.Window.WaitCamera") : QTStr("Right.Window.Expired");
	}
	// ���¸��´��ڱ���...
	this->update();
}

// �����߳�ע��ѧ���˳ɹ�֮��Ĳ���...
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
	// �����ʦ�����Ѿ�������ֱ�ӷ���...
	if (m_lpViewTeacher != NULL)
		return;
	// ������Ϣ��������ɫ...
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	// �Զ�������ʾ����...
	painter.drawText(this->rect(), Qt::AlignCenter, m_strNotice);
}

void CViewRight::doDestroyResource()
{
	// ɾ����ʦ�˴��ڶ���...
	if (m_lpViewTeacher != NULL) {
		delete m_lpViewTeacher;
		m_lpViewTeacher = NULL;
	}
	// ����Ĭ�ϵ���ʾ��Ϣ...
	m_strNotice = QTStr("Left.Window.DefaultNotice");
	// ���´��ڽ�����Ϣ...
	this->update();
}