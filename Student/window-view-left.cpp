
#include <qevent.h>
#include <assert.h>
#include <QPainter>
#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define COL_SIZE	1		// 默认列数...
#define	ROW_SIZE	4		// 默认行数...

CViewLeft::CViewLeft(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_nFirstID(0)
{
	m_bkColor = QColor(64, 64, 64);
}

CViewLeft::~CViewLeft()
{
	// 这里不删除的话QT也会自动删除...
	GM_MapCamera::iterator itorItem = m_MapCamera.begin();
	while (itorItem != m_MapCamera.end()) {
		CViewCamera * lpViewCamera = itorItem->second;
		delete lpViewCamera; lpViewCamera = NULL;
		m_MapCamera.erase(itorItem++);
	}
}

void CViewLeft::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setBrush(QBrush(m_bkColor));
	painter.drawRect(this->rect());
	if (m_MapCamera.size() > 0)
		return;
	assert(m_MapCamera.size() <= 0);
	QFont theFont = this->font();
	theFont.setPointSize(10);
	this->setFont(theFont);
	painter.setPen(QPen(QColor(255,255,0)));
	painter.drawText(this->rect(), Qt::AlignCenter, QStringLiteral("请添加通道"));

	//QWidget::paintEvent(event);
}

void CViewLeft::resizeEvent(QResizeEvent *event)
{
	int nColSize = COL_SIZE;
	int nRowSize = ROW_SIZE;
	QSize theSize = event->size();
	if( theSize.width() > 0 && theSize.height() > 0 ) {
		theSize.setWidth(theSize.width() / nColSize);
		theSize.setHeight(theSize.height() / nRowSize);
		this->LayoutViewCamera(theSize);
	}
	QWidget::resizeEvent(event);
}

void CViewLeft::LayoutViewCamera(QSize & inSize)
{
	// 找到第一个窗口的算子对象 => map对象已经是降序排列...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
	if (itorItem == m_MapCamera.end())
		return;
	// 将第一个窗口对象保存起来...
	CViewCamera * lpFirstWnd = NULL;
	lpFirstWnd = itorItem->second;
	if (lpFirstWnd == NULL)
		return;
	assert(lpFirstWnd != NULL);
	// 先将第一个算子前面的窗口都隐藏起来...
	GM_MapCamera::iterator itorBegin = m_MapCamera.begin();
	while (itorBegin != itorItem) {
		if (itorBegin->second != NULL) {
			itorBegin->second->hide();
		}
		++itorBegin;
	}
	// 计算需要的行、列数字...
	int nCol = COL_SIZE;
	int nRow = ROW_SIZE;
	int	nWidth = inSize.width();
	int	nHigh = inSize.height();
	for (int i = 0; i < nRow; ++i) {
		for (int j = 0; j < nCol; ++j) {
			// 达到最后一个窗口，退出前将第一个窗口设置成焦点窗口...
			if (itorItem == m_MapCamera.end()) {
				//lpFirstWnd->doCaptureFocus();
				return;
			}
			// 视频窗口无效，继续下一个窗口...
			CViewCamera * lpViewCamera = itorItem->second;
			if (lpViewCamera == NULL) {
				++itorItem;
				continue;
			}
			// 把当前窗口显示出来...
			lpViewCamera->show();
			// 调整窗口开始位置...
			int	nTop = i * nHigh;
			int	nLeft = j * nWidth;
			lpViewCamera->setGeometry(nLeft, nTop, nWidth, nHigh);
			// 然后，检测下一个窗口...
			++itorItem;
		}
	}
	// 接着，将从末尾窗口到最后窗口都隐藏起来...
	while (itorItem != m_MapCamera.end()) {
		if (itorItem->second != NULL) {
			itorItem->second->hide();
		}
		++itorItem;
	}
	// 最后，将第一个窗口设置成焦点窗口...
	//ASSERT(lpFirstWnd != NULL);
	//lpFirstWnd->doFocusAction();
}
