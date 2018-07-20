
#include <QPainter>
#include "window-view-teacher.hpp"

#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	// ���ñ���������������Ϣ...
	//QString::fromLocal8Bit("��ʦ�˻���");
	m_strTitleBase = QStringLiteral("��ʦ�˻���");
	m_strTitleText = m_strTitleBase;
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

CViewTeacher::~CViewTeacher()
{
}

void CViewTeacher::paintEvent(QPaintEvent *event)
{
	// ���Ʊ���������...
	this->DrawTitleArea();
	// ����������֮��Ĵ�������...
	this->DrawRenderArea();
	
	//QWidget::paintEvent(event);
}

void CViewTeacher::setTitleContent(QString & titleContent)
{
	// ������������������ֽ�����ϸ�ʽ������...
	m_strTitleText = QString("%1 - %2").arg(m_strTitleBase).arg(titleContent);
	// ������ʾ�ӿڣ�ֻ���±�������������...
	this->DrawTitleArea();
}

void CViewTeacher::DrawTitleArea()
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
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT)/2;
	painter.drawText(nPosX, nPosY, m_strTitleText);
}

void CViewTeacher::DrawRenderArea()
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

void CViewTeacher::mousePressEvent(QMouseEvent *event)
{
	// �������¼���� => �������¼�...
	if (event->button() == Qt::LeftButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewTeacher::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewTeacher::onFullScreenAction()
{
	if( this->isFullScreen() ) {
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
}

void CViewTeacher::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
	}
}