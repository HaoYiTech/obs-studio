
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
{
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

void OBSQTDisplay::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
}

void OBSQTDisplay::mouseDoubleClickEvent(QMouseEvent *event)
{
	// 双击全屏当前选中的数据源显示对象...
	//OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	//main->DoDisplayDbClicked();
}