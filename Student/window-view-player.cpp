
#include <QPainter>
#include "student-app.h"
#include "window-view-player.hpp"
#include "window-view-camera.hpp"

#define NOTICE_FONT_HEIGHT		12
#define NOTICE_TEXT_COLOR		QColor(255,255,0)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)

CViewPlayer::CViewPlayer(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_bIsChangeScreen(false)
  , m_bRectChanged(false)
  , m_bIsDrawImage(false)
  , m_hRenderWnd(NULL)
{
	// 保存渲染窗口句柄对象...
	m_hRenderWnd = (HWND)this->winId();
	// 强制转换父窗口对象...
	m_lpViewCamera = qobject_cast<CViewCamera*>(parent);
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// 设置窗口字体大小...
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(NOTICE_FONT_HEIGHT);
	theFont.setWeight(QFont::Light);
	this->setFont(theFont);
}

CViewPlayer::~CViewPlayer()
{
}

void CViewPlayer::doUpdateNotice(const QString & strNotice, bool bIsDrawImage/* = false*/)
{
	m_bIsDrawImage = bIsDrawImage;
	m_strNoticeText = strNotice;
	this->update();
}

void CViewPlayer::paintEvent(QPaintEvent *event)
{
	// 如果正在用SDL绘制图片，忽略事件，直接返回...
	if (m_bIsDrawImage) {
		event->ignore();
		return;
	}
	// 准备通用绘制对象...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// 先填充背景单纯颜色...
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
	// 设置信息栏文字颜色...
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	// 自动居中显示文字...
	painter.drawText(this->rect(), Qt::AlignCenter, m_strNoticeText);
}

// 重置窗口的矩形区域 => 相对父窗口的矩形坐标...
void CViewPlayer::doResizeRect(const QRect & rcInRect)
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
	m_bRectChanged = true;
}

// 返回矩形区域是否发生变化，并重置标志...
bool CViewPlayer::GetAndResetRenderFlag()
{
	bool bRetFlag = m_bRectChanged;
	m_bRectChanged = false;
	return bRetFlag;
}

void CViewPlayer::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewPlayer::onFullScreenAction()
{
	// 设置正在处理屏幕变化标志...
	m_bIsChangeScreen = true;
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
		m_bRectChanged = true;
	}
	// 还原屏幕变化标志...
	m_bIsChangeScreen = false;
}

void CViewPlayer::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		// 设置正在处理屏幕变化标志...
		m_bIsChangeScreen = true;
		// 还原渲染窗口的状态 => 恢复到普通窗口...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// 比对新的矩形与保留矩形是否发生变化...
		if (m_rcRenderRect != this->rect()) {
			m_rcRenderRect = this->rect();
			m_bRectChanged = true;
		}
		// 还原屏幕变化标志...
		m_bIsChangeScreen = false;
	}
}