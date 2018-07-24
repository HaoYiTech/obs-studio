
#include <qevent.h>
#include <assert.h>
#include <QPainter>

#include "student-app.h"
#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define COL_SIZE			1		// 默认列数...
#define	ROW_SIZE			4		// 默认行数...
#define NOTICE_FONT_SIZE	12		// 字体大小...
#define NOTICE_TEXT_COLOR	QColor(255,255,0)

CViewLeft::CViewLeft(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
  , m_nCurPage(0)
  , m_nMaxPage(0)
  , m_nFirstID(0)
  , m_nFocusID(-1)
{
	m_bkColor = QColor(64, 64, 64);
	QFont theFont = this->font();
	theFont.setFamily(QTStr("Student.Font.Family"));
	theFont.setPointSize(NOTICE_FONT_SIZE);
	this->setFont(theFont);
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
	painter.setPen(QPen(NOTICE_TEXT_COLOR));
	painter.drawText(this->rect(), Qt::AlignCenter, QTStr("Left.Window.DefaultNotice"));
}

void CViewLeft::resizeEvent(QResizeEvent *event)
{
	QSize theSize = event->size();
	if( theSize.width() > 0 && theSize.height() > 0 ) {
		this->LayoutViewCamera(theSize.width(), theSize.height());
	}
	QWidget::resizeEvent(event);
}

void CViewLeft::LayoutViewCamera(int cx, int cy)
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
	int	nWidth = cx / nCol;
	int	nHigh  = cy / nRow;
	for (int i = 0; i < nRow; ++i) {
		for (int j = 0; j < nCol; ++j) {
			// 达到最后一个窗口，退出前将第一个窗口设置成焦点窗口...
			if (itorItem == m_MapCamera.end()) {
				lpFirstWnd->doCaptureFocus();
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
	ASSERT(lpFirstWnd != NULL);
	lpFirstWnd->doCaptureFocus();
}

// 网络线程注册学生端成功之后的操作...
void CViewLeft::onWebLoadResource()
{
	GM_MapNodeCamera::iterator itorItem;
	GM_MapNodeCamera & theMapCamera = App()->GetNodeCamera();
	// 创建网站配置通道列表 => 按照从小到大的顺序创建...
	for (itorItem = theMapCamera.begin(); itorItem != theMapCamera.end(); ++itorItem) {
		GM_MapData & theData = itorItem->second;
		this->BuildWebCamera(theData);
	}
	// 对窗口进行重排显示...
	QRect & rcRect = this->rect();
	this->LayoutViewCamera(rcRect.width(), rcRect.height());
	// 根据页面数设置菜单状态...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
	// 设置其它通用菜单状态...
	emit this->enableCameraAdd(true);
	emit this->enableSettingSystem(true);
	emit this->enableSettingReconnect(true);
}

void CViewLeft::BuildWebCamera(GM_MapData & inWebData)
{
	GM_MapData::iterator itorID, itorName;
	itorID = inWebData.find("camera_id");
	itorName = inWebData.find("camera_name");
	if ((itorID == inWebData.end()) || (itorName == inWebData.end())) {
		MsgLogGM(GM_No_Xml_Node);
		return;
	}
	// 计算每页窗口数 => 列 * 行...
	int nPerPageSize = COL_SIZE * ROW_SIZE;
	int nDBCameraID = atoi(itorID->second.c_str());
	string & strCameraName = itorName->second;
	// 创建一个新的视频窗口 => 设置标题栏文字信息...
	CViewCamera * lpViewCamera = new CViewCamera(this, nDBCameraID);
	m_MapCamera[nDBCameraID] = lpViewCamera;
	// 将窗口标题栏先从UTF8转换成宽字符集，再从宽字符集转换成QString...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	os_utf8_to_wcs(strCameraName.c_str(), strCameraName.size(), szWBuffer, MAX_PATH);
	lpViewCamera->setTitleContent(QString((QChar*)szWBuffer));
	// 计算总页数 => 注意加1的情况...
	int nTotalWnd = m_MapCamera.size();
	m_nMaxPage = nTotalWnd / nPerPageSize;
	m_nMaxPage += ((nTotalWnd % nPerPageSize) ? 1 : 0);
	// map对象按窗口编号降序排列 => 获取它的数据库编号...
	m_nFirstID = m_MapCamera.begin()->first;
	// 设定当前页为第一页，因为有新窗口，需要重新开始...
	m_nCurPage = 1;
}

void CViewLeft::wheelEvent(QWheelEvent *event)
{
	// 向上滚动...
	if (event->delta() > 0) {
		this->onPagePrev();
	}
	// 向下滚动...
	if (event->delta() < 0) {
		this->onPageNext();
	}
}

void CViewLeft::onPagePrev()
{
	do {
		// 判断当前页码是否有效...
		if (m_nCurPage <= 1)
			break;
		// 找到第一个窗口的算子对象 => map对象已经是降序排列...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
		if (itorItem == m_MapCamera.end())
			break;
		// 获取每页大小 => 列 * 行...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// 当前页向前推进一页...
		m_nCurPage -= 1;
		// 算子向前推进一页 => 使用STL扩展...
		std::advance(itorItem, -nPerPageSize);
		// 得到最新的第一个窗口编号...
		if (itorItem == m_MapCamera.end())
			break;
		m_nFirstID = itorItem->first;
		// 对子窗口进行布局重绘操作...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// 根据页面数设置菜单状态...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}

void CViewLeft::onPageNext()
{
	do {
		// 判断当前页码是否有效...
		if (m_nCurPage >= m_nMaxPage)
			break;
		// 找到第一个窗口的算子对象 => map对象已经是降序排列...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
		if (itorItem == m_MapCamera.end())
			break;
		// 获取每页大小 => 列 * 行...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// 当前页向后推进一页...
		m_nCurPage += 1;
		// 算子向后推进一页 => 使用STL扩展...
		std::advance(itorItem, nPerPageSize);
		// 得到最新的第一个窗口编号...
		if (itorItem == m_MapCamera.end())
			break;
		m_nFirstID = itorItem->first;
		// 对子窗口进行布局重绘操作...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// 根据页面数设置菜单状态...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}

void CViewLeft::onPageJump()
{
}

void CViewLeft::doEnableCamera(OBSQTDisplay * lpNewDisplay)
{
	// 重置当前焦点摄像头的窗口编号...
	CViewCamera * lpViewCamera = NULL;
	do {
		// 如果输入参数无效，直接返回...
		if (lpNewDisplay == NULL)
			break;
		// 获取当前焦点的类名称...
		QString strClass = lpNewDisplay->metaObject()->className();
		bool bIsViewCamera = ((strClass == QStringLiteral("CViewCamera")) ? true : false);
		// 更新窗口的修改、删除菜单...
		emit this->enableCameraMod(bIsViewCamera);
		emit this->enableCameraDel(bIsViewCamera);
		// 如果不是摄像头窗口对象，直接返回...
		if (!bIsViewCamera)
			break;
		// 获取摄像头窗口对象的运行状态...
		lpViewCamera = qobject_cast<CViewCamera*>(lpNewDisplay);
	} while (false);
	// 如果当前摄像头窗口对象无效，重置菜单和焦点编号...
	if (lpViewCamera == NULL) {
		emit this->enableCameraStart(false);
		emit this->enableCameraStop(false);
		m_nFocusID = -1;
		return;
	}
	// 保存当前焦点摄像头的窗口编号...
	m_nFocusID = lpViewCamera->GetDBCameraID();
	// 根据摄像头窗口登录状态设置开始|停止菜单状态...
	bool bIsLogin = lpViewCamera->IsCameraLogin();
	emit this->enableCameraStart(!bIsLogin);
	emit this->enableCameraStop(bIsLogin);
}

CViewCamera * CViewLeft::FindDBCameraByID(int nDBCameraID)
{
	CViewCamera * lpViewCamera = NULL;
	do {
		if (nDBCameraID <= 0)
			break;
		GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
		if (itorItem == m_MapCamera.end())
			break;
		ASSERT(itorItem->first == nDBCameraID);
		lpViewCamera = itorItem->second;
	} while (false);
	return lpViewCamera;
}

void CViewLeft::onCameraStart()
{
	CViewCamera * lpViewCamera = this->FindDBCameraByID(m_nFocusID);
	if (lpViewCamera == NULL)
		return;
	bool bResult = lpViewCamera->doCameraStart();
	emit this->enableCameraStart(!bResult);
	emit this->enableCameraStop(bResult);
}

void CViewLeft::onCameraStop()
{
	CViewCamera * lpViewCamera = this->FindDBCameraByID(m_nFocusID);
	if (lpViewCamera == NULL)
		return;
	bool bResult = lpViewCamera->doCameraStop();
	emit this->enableCameraStart(bResult);
	emit this->enableCameraStop(!bResult);
}
