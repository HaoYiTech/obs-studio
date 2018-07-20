
#include <qevent.h>
#include <assert.h>
#include <QPainter>
#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define COL_SIZE	1		// Ĭ������...
#define	ROW_SIZE	4		// Ĭ������...

CViewLeft::CViewLeft(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_nFirstID(0)
{
	m_bkColor = QColor(64, 64, 64);
}

CViewLeft::~CViewLeft()
{
	// ���ﲻɾ���Ļ�QTҲ���Զ�ɾ��...
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
	painter.drawText(this->rect(), Qt::AlignCenter, QStringLiteral("�����ͨ��"));

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
	// �ҵ���һ�����ڵ����Ӷ��� => map�����Ѿ��ǽ�������...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
	if (itorItem == m_MapCamera.end())
		return;
	// ����һ�����ڶ��󱣴�����...
	CViewCamera * lpFirstWnd = NULL;
	lpFirstWnd = itorItem->second;
	if (lpFirstWnd == NULL)
		return;
	assert(lpFirstWnd != NULL);
	// �Ƚ���һ������ǰ��Ĵ��ڶ���������...
	GM_MapCamera::iterator itorBegin = m_MapCamera.begin();
	while (itorBegin != itorItem) {
		if (itorBegin->second != NULL) {
			itorBegin->second->hide();
		}
		++itorBegin;
	}
	// ������Ҫ���С�������...
	int nCol = COL_SIZE;
	int nRow = ROW_SIZE;
	int	nWidth = inSize.width();
	int	nHigh = inSize.height();
	for (int i = 0; i < nRow; ++i) {
		for (int j = 0; j < nCol; ++j) {
			// �ﵽ���һ�����ڣ��˳�ǰ����һ���������óɽ��㴰��...
			if (itorItem == m_MapCamera.end()) {
				//lpFirstWnd->doCaptureFocus();
				return;
			}
			// ��Ƶ������Ч��������һ������...
			CViewCamera * lpViewCamera = itorItem->second;
			if (lpViewCamera == NULL) {
				++itorItem;
				continue;
			}
			// �ѵ�ǰ������ʾ����...
			lpViewCamera->show();
			// �������ڿ�ʼλ��...
			int	nTop = i * nHigh;
			int	nLeft = j * nWidth;
			lpViewCamera->setGeometry(nLeft, nTop, nWidth, nHigh);
			// Ȼ�󣬼����һ������...
			++itorItem;
		}
	}
	// ���ţ�����ĩβ���ڵ���󴰿ڶ���������...
	while (itorItem != m_MapCamera.end()) {
		if (itorItem->second != NULL) {
			itorItem->second->hide();
		}
		++itorItem;
	}
	// ��󣬽���һ���������óɽ��㴰��...
	//ASSERT(lpFirstWnd != NULL);
	//lpFirstWnd->doFocusAction();
}
