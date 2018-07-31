
#include "student-app.h"
#include "qt-display.hpp"
#include "qt-wrappers.hpp"

#include <QWindow>
#include <QScreen>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>

OBSQTDisplay::OBSQTDisplay(QWidget *parent, Qt::WindowFlags flags)
  : QWidget(parent, flags)
  , m_bkColor(QColor(0,0,0))
  , m_bFocus(false)
{
	// 设置无标题窗口属性 => 默认就带这个属性？可以不用设置...
	//this->setWindowFlags(Qt::FramelessWindowHint);
	//this->setFocusPolicy(Qt::StrongFocus);
	/*setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);*/
}

void OBSQTDisplay::setBKColor(QColor inColor)
{
	m_bkColor = inColor;
	this->update();
}

/*void OBSQTDisplay::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
}*/

void OBSQTDisplay::doCaptureFocus()
{
	// 对摄像头控制菜单做更新 => 调用信号槽...
	emit App()->doEnableCamera(this);
	// 恢复之前的焦点窗口的状态...
	App()->doSaveFocus(this);
	// 设置当前窗口为焦点窗口...
	m_bFocus = true;
	// 计算并更新焦点裁剪区域...
	this->doUpdateFocusRegion();
}

void OBSQTDisplay::doUpdateFocusRegion()
{
	////////////////////////////////////////////////////////////////////////////
	// https://blog.csdn.net/u013255206/article/details/70312818
	// 计算重绘裁剪区域 => 参考例子是向外画，我们这里是向内画...
	////////////////////////////////////////////////////////////////////////////
	QRect rcRect = this->rect();
	QRegion oldRect(rcRect);
	rcRect = rcRect.adjusted(2, 2, -2, -2);
	QRegion newRect(rcRect);
	QRegion borderRect = newRect.xored(oldRect);
	// 更新重绘计算区域...
	this->update(borderRect);
}

void OBSQTDisplay::doReleaseFocus()
{
	// 当前窗口失去焦点...
	m_bFocus = false;
	// 计算并更新焦点裁剪区域...
	this->doUpdateFocusRegion();
}
