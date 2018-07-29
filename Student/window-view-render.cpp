
#include <QPainter>
#include "student-app.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define NOTICE_FONT_HEIGHT		30
#define NOTICE_TEXT_COLOR		QColor(255,255,0)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)

CViewRender::CViewRender(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_bIsChangeScreen(false)
  , m_bRectChanged(false)
  , m_hRenderWnd(NULL)
  , m_nStateTimer(-1)
{
	// ������Ⱦ���ھ������...
	m_hRenderWnd = (HWND)this->winId();
	// ǿ��ת�������ڶ���...
	m_lpViewTeacher = qobject_cast<CViewTeacher*>(parent);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ����ʱ�ӣ�ÿ������ˢ��״̬...
	m_nStateTimer = this->startTimer(500);
	// ���ô��������С...
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(NOTICE_FONT_HEIGHT);
	theFont.setWeight(QFont::Light);
	this->setFont(theFont);
}

CViewRender::~CViewRender()
{
}

// ʱ�Ӷ�ʱִ�й���...
void CViewRender::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nStateTimer) {
		this->update();
	}
}

void CViewRender::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ����Ѿ��յ���һ����Ƶ�ؼ�֡ => ɾ��ʱ��...
	if (m_lpViewTeacher->IsFindFirstVKey()) {
		if (m_nStateTimer >= 0) {
			this->killTimer(m_nStateTimer);
			m_nStateTimer = -1;
		}
		return;
	}
	// ��һ���ؼ�֡û�е��� => �鿴��¼״̬��׼������...
	int nCmdState = m_lpViewTeacher->GetCmdState();
	QString strNotice = QTStr((nCmdState <= 0) ? "Render.Window.DefaultNotice" : "Render.Window.WaitFirstVKey");
	// ����䱳��������ɫ...
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
	// ������Ϣ��������ɫ...
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	// �Զ�������ʾ����...
	painter.drawText(this->rect(), Qt::AlignCenter, strNotice);
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