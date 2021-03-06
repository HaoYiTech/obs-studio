
#include <QPainter>
#include "student-app.h"
#include "UDPRecvThread.h"
#include "UDPMultiRecvThread.h"
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
  , m_lpUDPMultiRecvThread(NULL)
  , m_lpUDPRecvThread(NULL)
  , m_lpViewRender(NULL)
{
	// 创建渲染窗口对象...
	QString strNotice = QTStr("Render.Window.TeacherNotice");
	m_lpViewRender = new CViewRender(strNotice, NOTICE_FONT_HEIGHT, this);
	// 设置标题栏基础文字信息...
	QString strRole = App()->GetRoleString();
	m_strTitleBase = QTStr("Teacher.Window.TitleBase");
	m_strTitleText = QString("%1 - %2").arg(m_strTitleBase).arg(strRole);
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	// 设置窗口字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

bool CViewTeacher::doRoleMultiRecv()
{
	// 如果不是组播转发角色，返回失败...
	if (App()->GetRoleType() != kRoleMultiRecv)
		return false;
	// 如果组播接收对象已经创建，直接返回...
	if (m_lpUDPMultiRecvThread != NULL)
		return false;
	// 得到终端所在房间编号，重建组播接收对象...
	int nRoomID = atoi(App()->GetRoomIDStr().c_str());
	m_lpUDPMultiRecvThread = new CUDPMultiRecvThread(m_lpViewRender);
	return m_lpUDPMultiRecvThread->InitThread();
}

bool CViewTeacher::doRoleMultiSend()
{
	// 如果是组播转发角色，返回失败...
	if (App()->GetRoleType() == kRoleMultiRecv)
		return false;
	// 如果UDP接收线程已经创建过了，直接返回...
	if (m_lpUDPRecvThread != NULL)
		return false;
	// 创建UDP接收线程，使用服务器传递过来的参数...
	int nUdpPort = App()->GetUdpPort();
	string & strUdpAddr = App()->GetUdpAddr();
	int nRoomID = atoi(App()->GetRoomIDStr().c_str());
	m_lpUDPRecvThread = new CUDPRecvThread(m_lpViewRender, nRoomID, 0);
	return m_lpUDPRecvThread->InitThread(strUdpAddr, nUdpPort);
}

// 响应从CRemoteSession发出的事件通知信号...
void CViewTeacher::onTriggerUdpRecvThread(bool bIsUDPTeacherOnLine)
{
	// 如果是老师推流端在线通知 => 创建拉流线程...
	if (bIsUDPTeacherOnLine) {
		// 根据终端模式，创建不同对象...
		bool bResult = false;
		switch (App()->GetRoleType())
		{
		case kRoleMultiRecv: bResult = this->doRoleMultiRecv(); break;
		case kRoleMultiSend: bResult = this->doRoleMultiSend(); break;
		case kRoleWanRecv:   bResult = this->doRoleMultiSend(); break;
		}
		// 如果重建失败，返回...
		if (!bResult) return;
		// 更新渲染界面窗口显示信息 => 讲师推流端已经上线...
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.TeacherOnLine"));
		// 判断是否处于全屏，如果不是全屏，进入全屏状态...
		if ( !m_lpViewRender->isFullScreen() ) {
			m_lpViewRender->onFullScreenAction();
		}
	} else {
		// 如果是老师推流端离线通知，删除组播线程...
		if (m_lpUDPMultiRecvThread != NULL) {
			delete m_lpUDPMultiRecvThread;
			m_lpUDPMultiRecvThread = NULL;
		}
		// 如果是老师推流端离线通知，删除拉流线程...
		if (m_lpUDPRecvThread != NULL) {
			delete m_lpUDPRecvThread;
			m_lpUDPRecvThread = NULL;
		}
		// 更新渲染界面窗口显示信息 => 恢复成默认提示信息...
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.TeacherNotice"));
	}
}

void CViewTeacher::doResetMulticastIPSend()
{
	// 如果组播接收对象有效，进行发送接口重置...
	if (m_lpUDPMultiRecvThread != NULL) {
		m_lpUDPMultiRecvThread->doResetMulticastIPSend();
	}
	// 如果接收线程有效，进行发送接口重置...
	if (m_lpUDPRecvThread != NULL) {
		m_lpUDPRecvThread->doResetMulticastIPSend();
	}
}

void CViewTeacher::onTriggerDeleteExAudioThread()
{
	// 如果组播接收对象有效，删除扩展音频播放线程...
	if (m_lpUDPMultiRecvThread != NULL) {
		m_lpUDPMultiRecvThread->doDeleteExAudioThread();
	}
	// 如果接收线程有效，删除扩展音频播放线程...
	if (m_lpUDPRecvThread != NULL) {
		m_lpUDPRecvThread->doDeleteExAudioThread();
	}
}

CViewTeacher::~CViewTeacher()
{
	// 删除UDP组播接收线程对象...
	if (m_lpUDPMultiRecvThread != NULL) {
		delete m_lpUDPMultiRecvThread;
		m_lpUDPMultiRecvThread = NULL;
	}
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
	// 准备通用绘制对象...
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// 获取窗口矩形区域...
	QRect rcRect = this->rect();
	// 如果正在用SDL绘制图片，需要进行区域裁剪...
	if (m_lpViewRender->IsDrawImage()) {
		QRect rcSrcRect = rcRect;
		QRegion oldRect(rcSrcRect);
		rcSrcRect = rcSrcRect.adjusted(2, 2, -2, -2);
		QRegion newRect(rcSrcRect);
		QRegion borderRect = newRect.xored(oldRect);
		painter.setClipRegion(borderRect);
	}
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
	// 渲染窗口有效，重置渲染窗口的矩形位置...
	m_lpViewRender->doResizeRect(rcRect);
}

void CViewTeacher::mousePressEvent(QMouseEvent *event)
{
	// 鼠标左键或右键事件点击 => 处理焦点事件...
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
	// 如果接收线程无效或渲染窗口无效或当前窗口不是焦点窗口，返回失败...
	if ((m_lpUDPRecvThread == NULL) || (m_lpViewRender == NULL) || (!this->IsFoucs()))
		return false;
	// 根据按键解析出放大或缩小状态...
	bool bIsVolPlus = false;
	switch (inKeyItem)
	{
	case Qt::Key_Less:  bIsVolPlus = false; break;
	case Qt::Key_Minus: bIsVolPlus = false; break;
	case Qt::Key_Equal: bIsVolPlus = true;  break;
	case Qt::Key_Plus:  bIsVolPlus = true;  break;
	default:			return false;
	}
	// 调用接收线程进行，键盘事件的投递操作，放大或缩小音量...
	return m_lpUDPRecvThread->doVolumeEvent(bIsVolPlus);
}