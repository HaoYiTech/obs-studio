
#include <QPainter>
#include "student-app.h"
#include "UDPRecvThread.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define NOTICE_FONT_HEIGHT		30
#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_lpUDPRecvThread(NULL)
  , m_lpViewRender(NULL)
{
	// ������Ⱦ���ڶ���...
	QString strNotice = QTStr("Render.Window.DefaultNotice");
	m_lpViewRender = new CViewRender(strNotice, NOTICE_FONT_HEIGHT, this);
	// ���ñ���������������Ϣ...
	m_strTitleBase = QTStr("Teacher.Window.TitleBase");
	m_strTitleText = m_strTitleBase;
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

// ��Ӧ��CRemoteSession�������¼�֪ͨ�ź�...
void CViewTeacher::onTriggerUdpRecvThread(bool bIsUDPTeacherOnLine)
{
	// �������ʦ����������֪ͨ => ���������߳�...
	if (bIsUDPTeacherOnLine) {
		// ���UDP�����߳��Ѿ��������ˣ�ֱ�ӷ���...
		if (m_lpUDPRecvThread != NULL)
			return;
		ASSERT(m_lpUDPRecvThread == NULL);
		// ����UDP�����̣߳�ʹ�÷��������ݹ����Ĳ���...
		int nRoomID = atoi(App()->GetRoomIDStr().c_str());
		string & strUdpAddr = App()->GetUdpAddr();
		int nUdpPort = App()->GetUdpPort();
		m_lpUDPRecvThread = new CUDPRecvThread(m_lpViewRender, nRoomID, 0);
		m_lpUDPRecvThread->InitThread(strUdpAddr, nUdpPort);
		// ������Ⱦ���洰����ʾ��Ϣ => ��ʦ�������Ѿ�����...
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.TeacherOnLine"));
	} else {
		// �������ʦ����������֪ͨ => ɾ�������߳�...
		if (m_lpUDPRecvThread != NULL) {
			delete m_lpUDPRecvThread;
			m_lpUDPRecvThread = NULL;
		}
		// ������Ⱦ���洰����ʾ��Ϣ => �ָ���Ĭ����ʾ��Ϣ...
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.DefaultNotice"));
	}
}

CViewTeacher::~CViewTeacher()
{
	// ɾ��UDP�����̶߳���...
	if (m_lpUDPRecvThread != NULL) {
		delete m_lpUDPRecvThread;
		m_lpUDPRecvThread = NULL;
	}
	// ɾ����Ⱦ���ڶ���...
	if (m_lpViewRender != NULL) {
		delete m_lpViewRender;
		m_lpViewRender = NULL;
	}
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
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT)/2 + 1;
	painter.drawText(nPosX, nPosY, m_strTitleText);
}

void CViewTeacher::DrawRenderArea()
{
	// ׼��ͨ�û��ƶ���...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ��ȡ���ھ�������...
	QRect rcRect = this->rect();
	// ���������SDL����ͼƬ����Ҫ��������ü�...
	if (m_lpViewRender->IsDrawImage()) {
		QRect rcSrcRect = rcRect;
		QRegion oldRect(rcSrcRect);
		rcSrcRect = rcSrcRect.adjusted(2, 2, -2, -2);
		QRegion newRect(rcSrcRect);
		QRegion borderRect = newRect.xored(oldRect);
		painter.setClipRegion(borderRect);
	}
	// ���������α߿�...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// ע�⣺������ƾ��ε���ʼ�����-1��ʼ��QSplitter�Ķ���������(1,1)...
	// ���Ʊ߿����û�����ɫ�����ž�������...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// ����������Ⱦ������ο��С...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	// ����Ⱦ����ռλ������Ҫ��������...
	//painter.fillRect(rcRect, m_bkColor);
	// ע�⣺����ʹ��PS��ͼ��Ŵ�ȷ�ϴ�С...
	// ��Ⱦ������Ч��������Ⱦ���ڵľ���λ��...
	m_lpViewRender->doResizeRect(rcRect);
}

void CViewTeacher::mousePressEvent(QMouseEvent *event)
{
	// ���������Ҽ��¼���� => �������¼�...
	Qt::MouseButton theBtn = event->button();
	if (theBtn == Qt::LeftButton || theBtn == Qt::RightButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewTeacher::onFullScreenAction()
{
	if (m_lpViewRender == NULL)
		return;
	m_lpViewRender->onFullScreenAction();
}

bool CViewTeacher::doVolumeEvent(int inKeyItem)
{
	// ��������߳���Ч����Ⱦ������Ч��ǰ���ڲ��ǽ��㴰�ڣ�����ʧ��...
	if ((m_lpUDPRecvThread == NULL) || (m_lpViewRender == NULL) || (!this->IsFoucs()))
		return false;
	// ���ݰ����������Ŵ����С״̬...
	bool bIsVolPlus = false;
	switch (inKeyItem)
	{
	case Qt::Key_Less:  bIsVolPlus = false; break;
	case Qt::Key_Minus: bIsVolPlus = false; break;
	case Qt::Key_Equal: bIsVolPlus = true;  break;
	case Qt::Key_Plus:  bIsVolPlus = true;  break;
	default:			return false;
	}
	// ���ý����߳̽��У������¼���Ͷ�ݲ������Ŵ����С����...
	return m_lpUDPRecvThread->doVolumeEvent(bIsVolPlus);
}