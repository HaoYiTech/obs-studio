
#include <QPainter>
#include "student-app.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define WINDOW_BK_COLOR		QColor(32, 32, 32)

CViewRender::CViewRender(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_bIsChangeScreen(false)
  , m_bRectChanged(false)
  , m_hRenderWnd(NULL)
{
	// ������Ⱦ���ھ������...
	m_hRenderWnd = (HWND)this->winId();
	// ǿ��ת�������ڶ���...
	m_lpViewTeacher = qobject_cast<CViewTeacher*>(parent);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
}

CViewRender::~CViewRender()
{
}

void CViewRender::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
}

// ���ô��ڵľ������� => ��Ը����ڵľ�������...
void CViewRender::doResizeRect(const QRect & rcInRect)
{
	// ȫ��״̬�£�ֱ�ӷ���...
	if (this->isFullScreen())
		return;
	// �������λ��û�б仯��ֱ�ӷ��� => ��������Ը�����...
	const QRect & rcCurRect = this->geometry();
	if (rcCurRect == rcInRect)
		return;
	// ������Ⱦ���ڵľ�������...
	this->setGeometry(rcInRect);
	// ���滭��ʵ����Ⱦ�������� => ��Ա��ش���...
	m_rcRenderRect = this->rect();
	m_bRectChanged = true;
}

// ���ؾ��������Ƿ����仯�������ñ�־...
bool CViewRender::GetAndResetRenderFlag()
{
	bool bRetFlag = m_bRectChanged;
	m_bRectChanged = false;
	return bRetFlag;
}

void CViewRender::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewRender::onFullScreenAction()
{
	// �������ڴ�����Ļ�仯��־...
	m_bIsChangeScreen = true;
	if (this->isFullScreen()) {
		// �����˳�ȫ��״̬...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// ��Ҫ�ָ���ȫ��ǰ�ľ�������...
		this->setGeometry(m_rcNoramlRect);
	} else {
		// ��Ҫ�ȱ���ȫ��ǰ�ľ�������...
		m_rcNoramlRect = this->geometry();
		// ���ڽ���ȫ��״̬...
		this->setWindowFlags(Qt::Window);
		this->showFullScreen();
	}
	// �ȶ��µľ����뱣�������Ƿ����仯...
	if (m_rcRenderRect != this->rect()) {
		m_rcRenderRect = this->rect();
		m_bRectChanged = true;
	}
	// ��ԭ��Ļ�仯��־...
	m_bIsChangeScreen = false;
}

void CViewRender::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		// �������ڴ�����Ļ�仯��־...
		m_bIsChangeScreen = true;
		// ��ԭ��Ⱦ���ڵ�״̬ => �ָ�����ͨ����...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// �ȶ��µľ����뱣�������Ƿ����仯...
		if (m_rcRenderRect != this->rect()) {
			m_rcRenderRect = this->rect();
			m_bRectChanged = true;
		}
		// ��ԭ��Ļ�仯��־...
		m_bIsChangeScreen = false;
	}
}