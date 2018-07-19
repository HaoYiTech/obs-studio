
#include <QPainter>
#include "window-view-teacher.hpp"

#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)

CViewTeacher::CViewTeacher(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	// 设置标题栏基础文字信息...
	m_strTitleBase = QStringLiteral("讲师端画面");
	m_strTitleText = m_strTitleBase;
	// 初始化背景颜色和原始区域...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcNoramlRect.setRect(0, 0, 0, 0);
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// 设置窗口字体大小...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

CViewTeacher::~CViewTeacher()
{
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

void CViewTeacher::DrawRenderArea()
{
	QPainter painter(this);
	QRect rcRect = this->rect();
	// 注意：这里绘制矩形的起始坐标有点特殊...
	// 绘制边框，设置画笔颜色，缩放矩形区域...
	painter.setPen(QPen(TITLE_BK_COLOR, 2));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// 填充渲染区矩形框...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, -1, -1);
	painter.fillRect(rcRect, m_bkColor);
	// 注意：这里使用PS截图后放大确认大小...
	// 保存画面实际渲染矩形区域...
	m_rcRenderRect = rcRect;
}

void CViewTeacher::DrawTitleArea()
{
	QPainter painter(this);
	// 获取整个窗口客户区矩形大小...
	QRect & rcRect = this->rect();
	// 填充标题栏矩形区背景色...
	painter.fillRect(0, 0, rcRect.width(), TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// 设置标题栏文字颜色...
	painter.setPen(TITLE_TEXT_COLOR);
	// 计算标题栏坐标 => 特别注意：是文字左下角坐标...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT)/2;
	painter.drawText(nPosX, nPosY, m_strTitleText);
}

void CViewTeacher::mouseDoubleClickEvent(QMouseEvent *event)
{
	this->onFullScreenAction();
}

void CViewTeacher::onFullScreenAction()
{
	if( this->isFullScreen() ) {
		// 窗口退出全屏状态...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// 需要恢复到全屏前的矩形区域...
		this->setGeometry(m_rcNoramlRect);
	} else {
		// 需要先保存全屏前的矩形区域...
		m_rcNoramlRect = this->geometry();
		// 窗口进入全屏状态...
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