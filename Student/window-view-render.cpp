
#include <QPainter>
#include "student-app.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define WINDOW_BK_COLOR		QColor(32, 32, 32)

CViewRender::CViewRender(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_hRenderWnd(NULL)
{
	// 保存渲染窗口句柄对象...
	m_hRenderWnd = (HWND)this->winId();
	// 强制转换父窗口对象...
	m_lpViewTeacher = qobject_cast<CViewTeacher*>(parent);
	// 初始化背景颜色和原始区域...
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

// 重置窗口的矩形区域 => 相对父窗口的矩形坐标...
void CViewRender::doResizeRect(const QRect & rcInRect)
{
	// 全屏状态下，直接返回...
	if (this->isFullScreen())
		return;
	// 如果矩形位置没有变化，直接返回 => 坐标是相对父窗口...
	const QRect & rcCurRect = this->geometry();
	if (rcCurRect == rcInRect)
		return;
	// 重置渲染窗口的矩形区域...
	this->setGeometry(rcInRect);
	// 保存画面实际渲染矩形区域 => 相对本地窗口...
	m_rcRenderRect = this->rect();
	m_lpViewTeacher->ReInitSDLWindow();
}

void CViewRender::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewRender::onFullScreenAction()
{
	if (this->isFullScreen()) {
		// 窗口退出全屏状态...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// 需要恢复到全屏前的矩形区域...
		this->setGeometry(m_rcNoramlRect);
	} else {
		// 需要先保存全屏前的矩形区域...
		m_rcNoramlRect = this->geometry();
		// 窗口进入全屏状态...
		this->setWindowFlags(Qt::Window);
		this->showFullScreen();
	}
	// 比对新的矩形与保留矩形是否发生变化...
	if (m_rcRenderRect != this->rect()) {
		m_rcRenderRect = this->rect();
		m_lpViewTeacher->ReInitSDLWindow();
	}
}

void CViewRender::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		// 还原渲染窗口的状态 => 恢复到普通窗口...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// 比对新的矩形与保留矩形是否发生变化...
		if (m_rcRenderRect != this->rect()) {
			m_rcRenderRect = this->rect();
			m_lpViewTeacher->ReInitSDLWindow();
		}
	}
}