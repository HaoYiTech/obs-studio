
#include <QPainter>
#include "student-app.h"
#include "pull-thread.h"
#include "push-thread.h"

#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define LINE_SPACE_PIXEL		6
#define LEFT_SPACE_PIXEL		20
#define RIGHT_SPACE_PIXEL		20
#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)

CViewCamera::CViewCamera(QWidget *parent, int nDBCameraID)
  : OBSQTDisplay(parent, 0)
  , m_nDBCameraID(nDBCameraID)
  , m_lpPushThread(NULL)
  , m_lpViewLeft(NULL)
{
	ASSERT(m_nDBCameraID > 0 && parent != NULL);
	m_lpViewLeft = qobject_cast<CViewLeft*>(parent);
	ASSERT(parent != NULL && m_lpViewLeft != NULL);
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// 设置窗口标题栏字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

CViewCamera::~CViewCamera()
{
	// 释放推流数据管理器...
	if (m_lpPushThread != NULL) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
	// 如果自己就是那个焦点窗口，需要重置...
	App()->doResetFocus(this);
}

void CViewCamera::paintEvent(QPaintEvent *event)
{
	// 绘制标题栏内容...
	this->DrawTitleArea();
	// 填充除标题栏之外的窗口区域...
	this->DrawRenderArea();
	// 显示通道的文字状态信息...
	this->DrawStatusText();
	//QWidget::paintEvent(event);
}

void CViewCamera::mousePressEvent(QMouseEvent *event)
{
	// 鼠标左键事件点击 => 处理焦点事件...
	if (event->button() == Qt::LeftButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewCamera::DrawTitleArea()
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
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT) / 2 + 1;
	// 先获取摄像头的标题名称，再进行字符串重组...
	QString strTitleContent = App()->GetCameraName(m_nDBCameraID);
	QString strTitleText = QString("%1%2 - %3").arg(QStringLiteral("ID：")).arg(m_nDBCameraID).arg(strTitleContent);
	painter.drawText(nPosX, nPosY, strTitleText);
}

void CViewCamera::DrawRenderArea()
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
	// 填充渲染区矩形框...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	painter.fillRect(rcRect, m_bkColor);
	// 注意：这里使用PS截图后放大确认大小...
	// 保存画面实际渲染矩形区域...
	m_rcRenderRect = rcRect;
}

void CViewCamera::DrawStatusText()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// 调整矩形区域和画笔颜色...
	QRect rcRect = this->rect();
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	painter.setPen(QPen(STATUS_TEXT_COLOR));
	QFontMetrics & fontMetrics = painter.fontMetrics();
	// 计算顶部间距...
	int nLineSpace = LINE_SPACE_PIXEL;
	int nTopSpace = (rcRect.height() - TITLE_FONT_HEIGHT * 5 - nLineSpace * 4) / 2 - 20;
	// 显示通道状态标题栏 => 特别注意：是文字左下角坐标...
	int xNamePos = rcRect.left() + LEFT_SPACE_PIXEL;
	int yNamePos = rcRect.top() + nTopSpace + TITLE_FONT_HEIGHT;
	QString strTitleName = QTStr("Camera.Window.StatName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示通道状态具体内容...
	int xDataPos = xNamePos + fontMetrics.width(strTitleName);
	int yDataPos = yNamePos;
	QString strDataValue = QTStr(this->IsCameraLogin() ? "Camera.Window.OnLine" : "Camera.Window.OffLine");
	painter.drawText(xDataPos, yDataPos, strDataValue);
	// 显示拉流地址标题栏 => 注意是左下角坐标...
	strTitleName = QTStr("Camera.Window.PullName");
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 设置文字对齐与自动换行...
	QRect rcText;
	QTextOption txtOption(Qt::AlignLeft);
	txtOption.setWrapMode(QTextOption::WrapAnywhere);
	// 计算拉流地址显示坐标位置 => 注意是左下角坐标...
	rcText.setTop(yNamePos - TITLE_FONT_HEIGHT - 2);
	rcText.setLeft(xNamePos + fontMetrics.width(strTitleName));
	rcText.setRight(rcRect.right() - RIGHT_SPACE_PIXEL);
	rcText.setBottom(rcText.top() + TITLE_FONT_HEIGHT * 4);
	// 获取拉流地址的具体内容...
	strDataValue = App()->GetCameraPullUrl(m_nDBCameraID);
	// 显示拉流地址信息，并累加下一行显示位置...
	painter.drawText(rcText, strDataValue, txtOption);
	yNamePos += TITLE_FONT_HEIGHT * 4 + nLineSpace;
	// 显示接收码率的标题栏...
	strTitleName = QTStr("Camera.Window.PullRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示接收码率的具体内容...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetRecvPullRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// 显示推流地址标题栏...
	strTitleName = QTStr("Camera.Window.PushName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示推流地址具体内容...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetStreamPushUrl();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// 显示推送码率标题栏...
	strTitleName = QTStr("Camera.Window.PushRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// 显示推送码率具体内容...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetSendPushRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
}

QString CViewCamera::GetRecvPullRate()
{
	// 获取接收码率的具体数字 => 直接从推流管理器当中获取...
	int nRecvKbps = ((m_lpPushThread != NULL) ? m_lpPushThread->GetRecvPullKbps() : 0);
	// 如果码率为负数，需要停止通道，同时，更新菜单...
	if ( nRecvKbps < 0 ) {
		// 先停止通道...
		this->doCameraStop();
		// 如果焦点窗口还是当前窗口，需要更新工具栏菜单状态...
		if (m_lpViewLeft->GetFocusID() == m_nDBCameraID) {
			emit m_lpViewLeft->enableCameraStart(true);
			emit m_lpViewLeft->enableCameraStop(false);
		}
		// 重置接收码率...
		nRecvKbps = 0;
	}
	// 返回接收码率的字符串内容...
	return QString("%1 Kbps").arg(nRecvKbps);
}

QString CViewCamera::GetSendPushRate()
{
	QString strRate("0 Kbps");
	return strRate;
}

QString CViewCamera::GetStreamPushUrl()
{
	//QString strRate("udp://edu.ihaoyi.cn:5252");
	QString strRate = QTStr("Camera.Window.None");
	return strRate;
}

bool CViewCamera::doCameraStart()
{
	// 如果推流对象有效，直接返回...
	if (m_lpPushThread != NULL)
		return true;
	// 重建推流对象管理器...
	ASSERT(m_lpPushThread == NULL);
	m_lpPushThread = new CPushThread(this);
	// 初始化失败，直接返回...
	if (!m_lpPushThread->InitThread()) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
		return false;
	}
	// 更新显示状态...
	this->update();
	return true;
}

bool CViewCamera::doCameraStop()
{
	// 直接删除推流管理器...
	if (m_lpPushThread != NULL) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
	// 更新显示状态...
	this->update();
	return true;
}