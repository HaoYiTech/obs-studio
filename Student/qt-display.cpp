
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
	// �����ޱ��ⴰ������ => Ĭ�Ͼʹ�������ԣ����Բ�������...
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

void OBSQTDisplay::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
}

void OBSQTDisplay::doCaptureFocus()
{
	// �ָ�֮ǰ�Ľ��㴰�ڵ�״̬...
	App()->doSaveFocus(this);
	// ���õ�ǰ����Ϊ���㴰��...
	m_bFocus = true;
	// �����ػ��¼�...
	this->update();
}

void OBSQTDisplay::doReleaseFocus()
{
	// ��ǰ����ʧȥ����...
	m_bFocus = false;
	// �����ػ��¼�...
	this->update();
}
