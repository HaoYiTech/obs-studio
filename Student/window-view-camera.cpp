
#include <QPainter>
#include "window-view-camera.hpp"

#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)

CViewCamera::CViewCamera(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

CViewCamera::~CViewCamera()
{
}

void CViewCamera::paintEvent(QPaintEvent *event)
{
	// ���Ʊ���������...
	this->DrawTitleArea();
	// ����������֮��Ĵ�������...
	this->DrawRenderArea();

	//QWidget::paintEvent(event);
}

void CViewCamera::mousePressEvent(QMouseEvent *event)
{
	// �������¼���� => �������¼�...
	if (event->button() == Qt::LeftButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewCamera::setTitleContent(QString & titleContent)
{
	// ������������������ֽ�����ϸ�ʽ������...
	m_strTitleText = QString("%1%2 - %3").arg(QStringLiteral("ID��")).arg(10).arg(titleContent);
	// ������ʾ�ӿڣ�ֻ���±�������������...
	this->DrawTitleArea();
}

void CViewCamera::DrawTitleArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ��ȡ�������ڿͻ������δ�С...
	QRect & rcRect = this->rect();
	// ������������������ɫ...
	painter.fillRect(1, 1, rcRect.width()-2, TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// ���ñ�����������ɫ...
	painter.setPen(TITLE_TEXT_COLOR);
	// ������������� => �ر�ע�⣺���������½�����...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT) / 2;
	painter.drawText(nPosX, nPosY, m_strTitleText);
}

void CViewCamera::DrawRenderArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	QRect rcRect = this->rect();
	// ���������α߿�...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// ע�⣺������ƾ��ε���ʼ�����-1��ʼ��QSplitter�Ķ���������(1,1)...
	// ���Ʊ߿����û�����ɫ�����ž�������...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// �����Ⱦ�����ο�...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	painter.fillRect(rcRect, m_bkColor);
	// ע�⣺����ʹ��PS��ͼ��Ŵ�ȷ�ϴ�С...
	// ���滭��ʵ����Ⱦ��������...
	m_rcRenderRect = rcRect;
}