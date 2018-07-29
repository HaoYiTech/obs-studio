
#include <QPainter>
#include "student-app.h"
#include "UDPRecvThread.h"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)

bool CViewTeacher::IsFindFirstVKey()
{
	return ((m_lpUDPRecvThread != NULL) ? m_lpUDPRecvThread->IsFindFirstVKey() : false);
}

int	CViewTeacher::GetCmdState()
{
	return ((m_lpUDPRecvThread != NULL) ? m_lpUDPRecvThread->GetCmdState() : 0);
}

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_lpUDPRecvThread(NULL)
  , m_lpViewRender(NULL)
{
	// 创建渲染窗口对象...
	m_lpViewRender = new CViewRender(this);
	// 设置标题栏基础文字信息...
	m_strTitleBase = QTStr("Teacher.Window.TitleBase");
	m_strTitleText = m_strTitleBase;
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	// 设置窗口字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
	// 创建UDP接收线程，使用服务器传递过来的参数...
	int nRoomID = atoi(App()->GetRoomIDStr().c_str());
	string & strUdpAddr = App()->GetUdpAddr();
	int nUdpPort = App()->GetUdpPort();
	m_lpUDPRecvThread = new CUDPRecvThread(m_lpViewRender, nRoomID, 0);
	m_lpUDPRecvThread->InitThread(strUdpAddr, nUdpPort);
}

CViewTeacher::~CViewTeacher()
{
	// 删除UDP接收线程对象...
	if (m_lpUDPRecvThread != NULL) {
		delete m_lpUDPRecvThread;
		m_lpUDPRecvThread = NULL;
	}
	// 删除渲染窗口对象...
	if (m_lpViewRender != NULL) {
		delete m_lpViewRender;
		m_lpViewRender = NULL;
	}
}

void CViewTeacher::paintEvent(QPaintEvent *event)
{
	// 绘制标题栏内容...
	this->DrawTitleArea();
	// 填充除标题栏之外的窗口区域...
	this->DrawRenderArea();
	
	//QWidget::paintEvent(event);
}

void CViewTeacher::setTitleContent(QString & titleContent)
{
	// 将输入文字与基础文字进行组合格式化处理...
	m_strTitleText = QString("%1 - %2").arg(m_strTitleBase).arg(titleContent);
	// 调用显示接口，只更新标题栏区域内容...
	this->DrawTitleArea();
}

void CViewTeacher::DrawTitleArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// 获取整个窗口客户区矩形大小...
	QRect & rcRect = this->rect();
	// 填充标题栏矩形区背景色...
	painter.fillRect(1, 1, rcRect.width()-2, TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// 设置标题栏文字颜色...
	painter.setPen(TITLE_TEXT_COLOR);
	// 计算标题栏坐标 => 特别注意：是文字左下角坐标...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT)/2 + 1;
	painter.drawText(nPosX, nPosY, m_strTitleText);
}

void CViewTeacher::DrawRenderArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	QRect rcRect = this->rect();
	// 绘制最大矩形边框...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// 注意：这里绘制矩形的起始坐标从-1开始，QSplitter的顶点坐标是(1,1)...
	// 绘制边框，设置画笔颜色，缩放矩形区域...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// 继续计算渲染对象矩形框大小...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	// 有渲染窗口占位，不需要填充矩形区...
	//painter.fillRect(rcRect, m_bkColor);
	// 注意：这里使用PS截图后放大确认大小...
	// 如果渲染窗口无效，直接返回...
	if (m_lpViewRender == NULL)
		return;
	// 渲染窗口有效，重置渲染窗口的矩形位置...
	m_lpViewRender->doResizeRect(rcRect);
}

void CViewTeacher::mousePressEvent(QMouseEvent *event)
{
	// 鼠标左键事件点击 => 处理焦点事件...
	if (event->button() == Qt::LeftButton) {
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
