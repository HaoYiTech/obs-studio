
#include <QPainter>
#include "student-app.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define NOTICE_TEXT_COLOR		QColor(255,255,0)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)

CViewRender::CViewRender(QString & strNotice, int nFontSize, QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_strNoticeText(strNotice)
  , m_bIsChangeScreen(false)
  , m_bRectChanged(false)
  , m_bIsDrawImage(false)
  , m_hRenderWnd(NULL)
{
	// ������Ⱦ���ھ������...
	m_hRenderWnd = (HWND)this->winId();
	// ǿ��ת�������ڶ���...
	//qobject_cast<CViewTeacher*>(parent);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(nFontSize);
	theFont.setWeight(QFont::Light);
	this->setFont(theFont);
}

CViewRender::~CViewRender()
{
}

void CViewRender::doUpdateNotice(const QString & strNotice, bool bIsDrawImage/* = false*/)
{
	m_bIsDrawImage = bIsDrawImage;
	m_strNoticeText = strNotice;
	this->update();
}

void CViewRender::paintEvent(QPaintEvent *event)
{
	// ���������SDL����ͼƬ�������¼���ֱ�ӷ���...
	if (m_bIsDrawImage) {
		event->ignore();
		return;
	}
	// ׼��ͨ�û��ƶ���...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ����䱳��������ɫ...
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
	// ������Ϣ��������ɫ...
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	// �Զ�������ʾ����...
	painter.drawText(this->rect(), Qt::AlignCenter, m_strNoticeText);
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
	int nKeyItem = event->key();
	if (nKeyItem == Qt::Key_Escape) {
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