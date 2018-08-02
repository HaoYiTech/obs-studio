
#include <qevent.h>
#include <assert.h>
#include <QPainter>

#include "student-app.h"
#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define COL_SIZE			1		// Ĭ������...
#define	ROW_SIZE			4		// Ĭ������...
#define NOTICE_FONT_SIZE	12		// �����С...
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
	// ���ﲻɾ���Ļ�QTҲ���Զ�ɾ��...
	this->doDestoryResource();
}

// ��Ӧ��CRemoteSession�������¼�֪ͨ�ź�...
void CViewLeft::onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID)
{
	GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
	if (itorItem == m_MapCamera.end())
		return;
	CViewCamera * lpViewCamera = itorItem->second;
	lpViewCamera->onTriggerUdpSendThread(bIsStartCmd, nDBCameraID);
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
	int	nWidth = cx / nCol;
	int	nHigh  = cy / nRow;
	for (int i = 0; i < nRow; ++i) {
		for (int j = 0; j < nCol; ++j) {
			// �ﵽ���һ�����ڣ��˳�ǰ����һ���������óɽ��㴰��...
			if (itorItem == m_MapCamera.end()) {
				lpFirstWnd->doCaptureFocus();
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
	ASSERT(lpFirstWnd != NULL);
	lpFirstWnd->doCaptureFocus();
}

// ɾ����������ͷ������Դ...
void CViewLeft::doDestoryResource()
{
	// ������ɾ�������Ѿ�����������ͷ����...
	GM_MapCamera::iterator itorItem = m_MapCamera.begin();
	while (itorItem != m_MapCamera.end()) {
		CViewCamera * lpViewCamera = itorItem->second;
		delete lpViewCamera; lpViewCamera = NULL;
		m_MapCamera.erase(itorItem++);
	}
	// ���½�����Ϣ...
	this->update();
}

// �����߳�ע��ѧ���˳ɹ�֮��Ĳ���...
void CViewLeft::onWebLoadResource()
{
	GM_MapNodeCamera::iterator itorItem;
	GM_MapNodeCamera & theMapCamera = App()->GetNodeCamera();
	// ������վ����ͨ���б� => ���մ�С�����˳�򴴽�...
	for (itorItem = theMapCamera.begin(); itorItem != theMapCamera.end(); ++itorItem) {
		GM_MapData & theData = itorItem->second;
		this->BuildWebCamera(theData);
	}
	// �Դ��ڽ���������ʾ...
	QRect & rcRect = this->rect();
	this->LayoutViewCamera(rcRect.width(), rcRect.height());
	// ����ҳ�������ò˵�״̬...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
	// ��������ͨ�ò˵�״̬...
	emit this->enableCameraAdd(true);
	emit this->enableSettingSystem(true);
	emit this->enableSettingReconnect(true);
	emit this->enableSystemFullscreen(true);
	// ˢ����౾��ҳ��...
	this->update();
}
//
// ��Ȩ���� => �����������˵�...
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
	// ����ÿҳ������ => �� * ��...
	int nPerPageSize = COL_SIZE * ROW_SIZE;
	int nDBCameraID = atoi(itorID->second.c_str());
	// ����һ���µ���Ƶ���ڣ����沢������ʾ����...
	CViewCamera * lpViewCamera = new CViewCamera(this, nDBCameraID);
	m_MapCamera[nDBCameraID] = lpViewCamera;
	lpViewCamera->update();
	// ������ҳ�� => ע���1�����...
	int nTotalWnd = m_MapCamera.size();
	m_nMaxPage = nTotalWnd / nPerPageSize;
	m_nMaxPage += ((nTotalWnd % nPerPageSize) ? 1 : 0);
	// map���󰴴��ڱ�Ž������� => ��ȡ�������ݿ���...
	m_nFirstID = m_MapCamera.begin()->first;
	// �趨��ǰҳΪ��һҳ����Ϊ���´��ڣ���Ҫ���¿�ʼ...
	m_nCurPage = 1;
	// �����½�������ͷ���ڶ���...
	return lpViewCamera;
}

CViewCamera * CViewLeft::AddNewCamera(GM_MapData & inWebData)
{
	// ����˽�нӿڴ�������ͷ���ڶ��� => ����ʧ�ܣ�ֱ�ӷ���...
	CViewCamera * lpViewCamera = this->BuildWebCamera(inWebData);
	if (lpViewCamera == NULL)
		return NULL;
	// �Դ��ڽ���������ʾ...
	ASSERT(lpViewCamera != NULL);
	QRect & rcRect = this->rect();
	this->LayoutViewCamera(rcRect.width(), rcRect.height());
	// ����ҳ�������ò˵�״̬...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
	// �����½�������ͷ���ڶ���...
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
	// ���Ϲ���...
	if (event->delta() > 0) {
		this->onPagePrev();
	}
	// ���¹���...
	if (event->delta() < 0) {
		this->onPageNext();
	}
}

void CViewLeft::onPagePrev()
{
	do {
		// �жϵ�ǰҳ���Ƿ���Ч...
		if (m_nCurPage <= 1)
			break;
		// �ҵ���һ�����ڵ����Ӷ��� => map�����Ѿ��ǽ�������...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
		if (itorItem == m_MapCamera.end())
			break;
		// ��ȡÿҳ��С => �� * ��...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// ��ǰҳ��ǰ�ƽ�һҳ...
		m_nCurPage -= 1;
		// ������ǰ�ƽ�һҳ => ʹ��STL��չ...
		std::advance(itorItem, -nPerPageSize);
		// �õ����µĵ�һ�����ڱ��...
		if (itorItem == m_MapCamera.end())
			break;
		m_nFirstID = itorItem->first;
		// ���Ӵ��ڽ��в����ػ����...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// ����ҳ�������ò˵�״̬...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}

void CViewLeft::onPageNext()
{
	do {
		// �жϵ�ǰҳ���Ƿ���Ч...
		if (m_nCurPage >= m_nMaxPage)
			break;
		// �ҵ���һ�����ڵ����Ӷ��� => map�����Ѿ��ǽ�������...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(m_nFirstID);
		if (itorItem == m_MapCamera.end())
			break;
		// ��ȡÿҳ��С => �� * ��...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// ��ǰҳ����ƽ�һҳ...
		m_nCurPage += 1;
		// ��������ƽ�һҳ => ʹ��STL��չ...
		std::advance(itorItem, nPerPageSize);
		// �õ����µĵ�һ�����ڱ��...
		if (itorItem == m_MapCamera.end())
			break;
		m_nFirstID = itorItem->first;
		// ���Ӵ��ڽ��в����ػ����...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// ����ҳ�������ò˵�״̬...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}

void CViewLeft::onPageJump()
{
}

void CViewLeft::doEnableCamera(OBSQTDisplay * lpNewDisplay)
{
	// ���õ�ǰ��������ͷ�Ĵ��ڱ��...
	CViewCamera * lpViewCamera = NULL;
	do {
		// ������������Ч��ֱ�ӷ���...
		if (lpNewDisplay == NULL)
			break;
		// ��ȡ��ǰ�����������...
		QString strClass = lpNewDisplay->metaObject()->className();
		bool bIsViewCamera = ((strClass == QStringLiteral("CViewCamera")) ? true : false);
		// ���´��ڵ��޸ġ�ɾ���˵�...
		emit this->enableCameraMod(bIsViewCamera);
		emit this->enableCameraDel(bIsViewCamera);
		// �����������ͷ���ڶ���ֱ�ӷ���...
		if (!bIsViewCamera)
			break;
		// ��ȡ����ͷ���ڶ��������״̬...
		lpViewCamera = qobject_cast<CViewCamera*>(lpNewDisplay);
	} while (false);
	// �����ǰ����ͷ���ڶ�����Ч�����ò˵��ͽ�����...
	if (lpViewCamera == NULL) {
		emit this->enableCameraStart(false);
		emit this->enableCameraStop(false);
		m_nFocusID = -1;
		return;
	}
	// ���浱ǰ��������ͷ�Ĵ��ڱ��...
	m_nFocusID = lpViewCamera->GetDBCameraID();
	// ��������ͷ���ڵ�¼״̬���ÿ�ʼ|ֹͣ�˵�״̬...
	bool bIsLogin = lpViewCamera->IsCameraLogin();
	emit this->enableCameraStart(!bIsLogin);
	emit this->enableCameraStop(bIsLogin);
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
// ע�⣺ɾ����������� => ����ɾ����ť �� ��վ��̨ɾ��...
void CViewLeft::onCameraDel(int nDBCameraID)
{
	do {
		// ͨ������Ҳ�������ͷ���ڶ���ֱ�ӷ���...
		GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
		if (itorItem == m_MapCamera.end())
			break;
		// �ҵ�����ͷ���ڶ���...
		CViewCamera * lpViewCamera = itorItem->second;
		ASSERT(lpViewCamera != NULL);
		// ����ÿҳ��С => �� * ��...
		int nPerPageSize = COL_SIZE * ROW_SIZE;
		// ����ɾ��ͨ�����ڷ�ҳ���� => ��Ҫ�����Լ��ļ���...
		int nTotalNum = std::distance(m_MapCamera.begin(), itorItem) + 1;
		int nThisPage = nTotalNum / nPerPageSize;
		nThisPage += ((nTotalNum % nPerPageSize) ? 1 : 0);
		// ɾ����Ӧ������ͷ���ڶ���...
		delete lpViewCamera;
		lpViewCamera = NULL;
		m_MapCamera.erase(itorItem);
		// ���û�д����ˣ�������ر���������...
		if (m_MapCamera.size() <= 0) {
			m_nCurPage = 0;	m_nMaxPage = 0;
			m_nFirstID = 0; this->update();
			break;
		}
		// ���¼��������ҳ�� => ע���1�����...
		int nTotalWnd = m_MapCamera.size();
		m_nMaxPage = nTotalWnd / nPerPageSize;
		m_nMaxPage += ((nTotalWnd % nPerPageSize) ? 1 : 0);
		// ע�⣺�������������ͨ����վ���е�ͨ��ɾ������...
		// ���ɾ����������ҳ���ڵ�ǰҳ����Ӱ�죬ֱ�ӷ���...
		if (nThisPage > m_nCurPage)
			break;
		ASSERT(nThisPage <= m_nCurPage);
		// �����ǰҳ�������ҳ����Ҫ����ǰҳ�������ҳ...
		if (m_nCurPage > m_nMaxPage) {
			m_nCurPage = m_nMaxPage;
		}
		// ��ʱ��ɾ����������ҳС�ڻ���ڵ�ǰҳ��ʹ�õ�ǰҳ�������Ŵ���...
		itorItem = m_MapCamera.begin();
		std::advance(itorItem, (m_nCurPage - 1)*nPerPageSize);
		m_nFirstID = itorItem->first;
		// �Ե�ǰҳ���Ӵ��ڽ������Ų���...
		QRect & rcRect = this->rect();
		this->LayoutViewCamera(rcRect.width(), rcRect.height());
	} while (false);
	// ����ҳ�������ò˵�״̬...
	emit this->enablePagePrev(m_nCurPage > 1);
	emit this->enablePageJump(m_nMaxPage > 1);
	emit this->enablePageNext(m_nCurPage < m_nMaxPage);
}