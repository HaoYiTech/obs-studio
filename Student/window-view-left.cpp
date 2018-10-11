
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
  , m_bCanAutoLink(true)
  , m_nLiveCameraID(-1)
  , m_nCurAutoID(-1)
  , m_nAutoTimer(-1)
  , m_nFocusID(-1)
  , m_nCurPage(0)
  , m_nMaxPage(0)
  , m_nFirstID(0)
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
	this->doDestoryResource();
}

// 响应CRemoteSession发出的连接服务器成功通知 => 汇报在线通道状态...
void CViewLeft::onTriggerConnected()
{
	// 获取到远程会话连接对象...
	GM_MapCamera::iterator itorItem;
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	// 遍历所有正在拉流的摄像头通道，汇报给服务器，...
	for(itorItem = m_MapCamera.begin(); itorItem != m_MapCamera.end(); ++itorItem) {
		CViewCamera * lpViewCamera = itorItem->second;
		// 如果当前摄像头对象正在拉取数据(已连接) => 发起汇报命令...
		if (lpRemoteSession != NULL && lpViewCamera->IsCameraOnLine()) {
			lpRemoteSession->doSendStartCameraCmd(itorItem->first);
		}
	}
	// 启动一个每隔两秒进行拉流检测的时钟对象 => 先删除原来的时钟...
	(m_nAutoTimer >= 0) ? this->killTimer(m_nAutoTimer) : NULL;
	m_nAutoTimer = this->startTimer(2 * 1000);
	// 不要立即开始自动连接，需要延时连接，避免互相争抢资源...
}

void CViewLeft::timerEvent(QTimerEvent *inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nAutoTimer) {
		this->doAutoLinkIPC();
	}
}

void CViewLeft::doAutoLinkIPC()
{
	// 如果摄像头列表对象为空，直接返回...
	if (m_MapCamera.size() <= 0)
		return;
	// 如果当前的自动标志为否，直接返回...
	if (!m_bCanAutoLink)
		return;
	ASSERT(m_bCanAutoLink);
	// 计算出需要重连的摄像头编号 => 用当前的编号查找下一个连接编号...
	m_nCurAutoID = this->GetNextAutoID(m_nCurAutoID);
	// 如果找到的摄像头编号无效，直接返回...
	if (m_nCurAutoID <= 0)
		return;
	// 通过新的摄像头编号找到摄像头视图对象...
	CViewCamera * lpViewCamera = this->FindDBCameraByID(m_nCurAutoID);
	if (lpViewCamera == NULL)
		return;
	// 调用接口直接启动摄像头通道，开始拉取数据流...
	bool bResult = lpViewCamera->doCameraStart();
	// 如果当前摄像头编号与焦点摄像头编号一致，更新主界面工具栏...
	if (m_nCurAutoID == m_nFocusID) {
		emit this->enableCameraStart(!bResult);
		emit this->enableCameraStop(bResult);
	}
}

// 得到下一个需要重连的窗口编号...
int CViewLeft::GetNextAutoID(int nCurDBCameraID)
{
	// 如果摄像头列表对象为空，返回-1...
	if (m_MapCamera.size() <= 0)
		return -1;
	// 将第一个摄像头对象做为备用节点对象...
	GM_MapCamera::iterator itorFirst = m_MapCamera.begin();
	// 如果输入的摄像头编号是无效的，返回第一个有效节点...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(nCurDBCameraID);
	if (itorItem == m_MapCamera.end()) {
		return itorFirst->first;
	}
	// 如果下一个节点是无效的，回滚到第一个节点...
	if ((++itorItem) == m_MapCamera.end()) {
		return itorFirst->first;
	}
	// 下一个节点有效，返回对应的编号...
	ASSERT(itorItem != m_MapCamera.end());
	return itorItem->first;
}

// 响应从CRemoteSession发出的事件通知信号...
void CViewLeft::onTriggerLiveStart(int nDBCameraID)
{
	// 如果正在推流的通道编号有效，停止通道...
	this->doCommonLiveStop(m_nLiveCameraID);
	// 在摄像头通道集合中查找指定的通道编号...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
	if (itorItem == m_MapCamera.end())
		return;
	// 执行摄像头通道的开启推流线程...
	CViewCamera * lpViewCamera = itorItem->second;
	lpViewCamera->doUdpSendThreadStart();
	m_nLiveCameraID = nDBCameraID;
	// 打印当前新的正在推流通道编号...
	blog(LOG_INFO, "== onTriggerLiveStart => LiveID: %d ==", m_nLiveCameraID);
}

// 响应从CRemoteSession发出的事件通知信号...
void CViewLeft::onTriggerLiveStop(int nDBCameraID)
{
	// 调用统一的停止推流通道接口...
	this->doCommonLiveStop(nDBCameraID);
	// 回应讲师端停止推流通道执行成功...
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	if (lpRemoteSession != NULL) {
		lpRemoteSession->doSendLiveStopCmd(nDBCameraID);
	}
	// 打印当前正在停止推流通道编号...
	blog(LOG_INFO, "== onTriggerLiveStop => LiveID: %d ==", nDBCameraID);
}

// 统一的停止正在推流通道停止命令接口函数...
void CViewLeft::doCommonLiveStop(int nDBCameraID)
{
	// 如果正在推流通道已经无效，直接返回...
	if (m_nLiveCameraID <= 0 || nDBCameraID <= 0 )
		return;
	// 如果通道编号与正在推流通道不一致，打印信息，然后返回...
	if (m_nLiveCameraID != nDBCameraID) {
		blog(LOG_INFO, "== doCommonLiveStop => Error, LiveID: %d, StopID: %d ==", m_nLiveCameraID, nDBCameraID);
		return;
	}
	ASSERT(m_nLiveCameraID == nDBCameraID);
	// 如果没有找到正在推流的对象，重置推流通道编号，返回...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
	if (itorItem == m_MapCamera.end()) {
		m_nLiveCameraID = -1;
		return;
	}
	// 找到了正在推流的对象，发起停止命令...
	CViewCamera * lpViewCamera = itorItem->second;
	lpViewCamera->doUdpSendThreadStop();
	// 重置正在推流编号...
	m_nLiveCameraID = -1;
	// 打印已经停止的指定通道编号...
	blog(LOG_INFO, "== doCommonLiveStop => StopID: %d ==", nDBCameraID);
}

// 停止指定的推流通道 => UDP命令 => 推流退出通知...
void CViewLeft::doUdpPusherLogout(int nDBCameraID)
{
	// 调用统一的停止推流通道接口...
	this->doCommonLiveStop(nDBCameraID);
	// 打印已经停止的指定通道编号...
	blog(LOG_INFO, "== doUdpPusherLogout => StopID: %d ==", nDBCameraID);
}

// 处理右侧播放线程已经停止通知 => 只通知正在推流通道...
void CViewLeft::onUdpRecvThreadStop()
{
	// 查找正在推流的通道对象，没找到直接返回...
	GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nLiveCameraID);
	if (itorItem == m_MapCamera.end())
		return;
	// 通知正在推流的通道，右侧播放线程已经停止...
	CViewCamera * lpViewCamera = itorItem->second;
	lpViewCamera->onUdpRecvThreadStop();
}

// 向正在推流的通道投递扬声器的音频数据内容...
void CViewLeft::doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nLiveCameraID);
	if (itorItem == m_MapCamera.end())
		return;
	// 通知正在推流的通道，扬声器的音频数据到达...
	CViewCamera * lpViewCamera = itorItem->second;
	lpViewCamera->doEchoCancel(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf);

	/*GM_MapCamera::iterator itorItem = m_MapCamera.begin();
	while (itorItem != m_MapCamera.end()) {
		CViewCamera * lpViewCamera = itorItem->second;
		lpViewCamera->doEchoCancel(lpBufData, nBufSize, nSampleRate, nChannelNum, msInSndCardBuf);
		++itorItem;
	}*/
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

// 删除所有摄像头对象资源...
void CViewLeft::doDestoryResource()
{
	// 遍历并删除所有已经创建的摄像头对象...
	GM_MapCamera::iterator itorItem = m_MapCamera.begin();
	while (itorItem != m_MapCamera.end()) {
		CViewCamera * lpViewCamera = itorItem->second;
		delete lpViewCamera; lpViewCamera = NULL;
		m_MapCamera.erase(itorItem++);
	}
	// 更新界面信息...
	this->update();
	// 删除自动重连时钟...
	if (m_nAutoTimer >= 0) {
		this->killTimer(m_nAutoTimer);
		m_nAutoTimer = -1;
	}
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
	emit this->enableSystemFullscreen(true);
	// 刷新左侧本地页面...
	this->update();
}
//
// 授权过期 => 调整工具栏菜单...
void CViewLeft::onWebAuthExpired()
{
	emit this->enableSettingSystem(true);
	emit this->enableSettingReconnect(true);
}

CViewCamera * CViewLeft::BuildWebCamera(GM_MapData & inWebData)
{
	GM_MapData::iterator itorID, itorName;
	itorID = inWebData.find("camera_id");
	itorName = inWebData.find("camera_name");
	if ((itorID == inWebData.end()) || (itorName == inWebData.end())) {
		MsgLogGM(GM_No_Xml_Node);
		return NULL;
	}
	// 计算每页窗口数 => 列 * 行...
	int nPerPageSize = COL_SIZE * ROW_SIZE;
	int nDBCameraID = atoi(itorID->second.c_str());
	// 创建一个新的视频窗口，保存并更新显示界面...
	CViewCamera * lpViewCamera = new CViewCamera(this, nDBCameraID);
	m_MapCamera[nDBCameraID] = lpViewCamera;
	lpViewCamera->update();
	// 计算总页数 => 注意加1的情况...
	int nTotalWnd = m_MapCamera.size();
	m_nMaxPage = nTotalWnd / nPerPageSize;
	m_nMaxPage += ((nTotalWnd % nPerPageSize) ? 1 : 0);
	// map对象按窗口编号降序排列 => 获取它的数据库编号...
	m_nFirstID = m_MapCamera.begin()->first;
	// 设定当前页为第一页，因为有新窗口，需要重新开始...
	m_nCurPage = 1;
	// 返回新建的摄像头窗口对象...
	return lpViewCamera;
}

CViewCamera * CViewLeft::AddNewCamera(GM_MapData & inWebData)
{
	// 调用私有接口创建摄像头窗口对象 => 创建失败，直接返回...
	CViewCamera * lpViewCamera = this->BuildWebCamera(inWebData);
	if (lpViewCamera == NULL)
		return NULL;
	// 对窗口进行重排显示...
	ASSERT(lpViewCamera != NULL);
	QRect & rcRect = this->rect();
	this->LayoutViewCamera(rcRect.width(), rcRect.height());
	// 根据页面数设置菜单状态...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
	// 返回新建的摄像头窗口对象...
	return lpViewCamera;
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
	bool bIsOffLine = lpViewCamera->IsCameraOffLine();
	emit this->enableCameraStart(bIsOffLine);
	emit this->enableCameraStop(!bIsOffLine);
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
//
// 注意：删除有两个入口 => 本地删除按钮 或 网站后台删除...
void CViewLeft::onCameraDel(int nDBCameraID)
{
	do {
		// 通过编号找不到摄像头窗口对象，直接返回...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
		if (itorItem == m_MapCamera.end())
			break;
		// 找到摄像头窗口对象...
		CViewCamera * lpViewCamera = itorItem->second;
		ASSERT(lpViewCamera != NULL);
		// 计算每页大小 => 列 * 行...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// 计算删除通道所在分页号码 => 需要包含自己的计数...
		int nTotalNum = std::distance(m_MapCamera.begin(), itorItem) + 1;
		int nThisPage = nTotalNum / nPerPageSize;
		nThisPage += ((nTotalNum % nPerPageSize) ? 1 : 0);
		// 删除对应的摄像头窗口对象...
		delete lpViewCamera;
		lpViewCamera = NULL;
		m_MapCamera.erase(itorItem);
		// 如果没有窗口了，重置相关变量，返回...
		if (m_MapCamera.size() <= 0) {
			m_nCurPage = 0;	m_nMaxPage = 0;
			m_nFirstID = 0; this->update();
			break;
		}
		// 重新计算最大总页数 => 注意加1的情况...
		int nTotalWnd = m_MapCamera.size();
		m_nMaxPage = nTotalWnd / nPerPageSize;
		m_nMaxPage += ((nTotalWnd % nPerPageSize) ? 1 : 0);
		// 注意：这种情况发生在通过网站进行的通道删除操作...
		// 如果删除窗口所在页大于当前页，不影响，直接返回...
		if (nThisPage > m_nCurPage)
			break;
		ASSERT(nThisPage <= m_nCurPage);
		// 如果当前页大于最大页，需要将当前页等于最大页...
		if (m_nCurPage > m_nMaxPage) {
			m_nCurPage = m_nMaxPage;
		}
		// 这时，删除窗口所在页小于或等于当前页，使用当前页进行重排窗口...
		itorItem = m_MapCamera.begin();
		std::advance(itorItem, (m_nCurPage - 1)*nPerPageSize);
		m_nFirstID = itorItem->first;
		// 对当前页的子窗口进行重排操作...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// 根据页面数设置菜单状态...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}