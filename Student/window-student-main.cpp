
#include <QMessageBox>
#include <QShowEvent>
#include <QDesktopWidget>
#include <QRect>
#include <QScreen>

#include "HYDefine.h"
#include "web-thread.h"
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-push.hpp"
#include "window-student-main.h"
#include "window-view-camera.hpp"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"

#define STARTUP_SEPARATOR   "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR 	"==== Shutting down =================================================="

StudentWindow::StudentWindow(QWidget *parent)
 : QMainWindow(parent)
{
	// ���ý������...
	m_ui.setupUi(this);
	// ���ô���ͼ�� => ������pngͼƬ => �����Щ��������ico��������Ͻ�ͼ���޷���ʾ...
	this->setWindowIcon(QIcon(":/res/images/student.png"));
	// ������������ʾ״̬...
	m_ui.actionSystemToolbar->setCheckable(true);
	m_ui.actionSystemToolbar->setChecked(true);
	// �����������˵��¼���Ӧ...
	m_ui.mainToolBar->addAction(m_ui.actionPagePrev);
	m_ui.mainToolBar->addAction(m_ui.actionPageJump);
	m_ui.mainToolBar->addAction(m_ui.actionPageNext);
	m_ui.mainToolBar->addSeparator();
	// ���÷�ҳ�˵���ťΪ������...
	m_ui.actionPagePrev->setDisabled(true);
	m_ui.actionPageJump->setDisabled(true);
	m_ui.actionPageNext->setDisabled(true);
	// ���ù��������˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enablePagePrev(bool)), m_ui.actionPagePrev, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePageJump(bool)), m_ui.actionPageJump, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePageNext(bool)), m_ui.actionPageNext, SLOT(setEnabled(bool)));
	// ����ȫ���˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionSystemFullscreen);
	m_ui.mainToolBar->addSeparator();
	// ����ͨ�������˵���ť������...
	m_ui.actionSystemFullscreen->setDisabled(true);
	// ����ȫ�������˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enableSystemFullscreen(bool)), m_ui.actionSystemFullscreen, SLOT(setEnabled(bool)));
	// ����ͨ�������˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionCameraStart);
	m_ui.mainToolBar->addAction(m_ui.actionCameraStop);
	m_ui.mainToolBar->addSeparator();
	// ����ͨ�������˵���ť������...
	m_ui.actionCameraStart->setDisabled(true);
	m_ui.actionCameraStop->setDisabled(true);
	// ����ͨ�������˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraStart(bool)), m_ui.actionCameraStart, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraStop(bool)), m_ui.actionCameraStop, SLOT(setEnabled(bool)));
	// ����ͨ���˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionCameraAdd);
	m_ui.mainToolBar->addAction(m_ui.actionCameraMod);
	m_ui.mainToolBar->addAction(m_ui.actionCameraDel);
	m_ui.mainToolBar->addSeparator();
	// ����ͨ���˵���ťΪ������...
	m_ui.actionCameraAdd->setDisabled(true);
	m_ui.actionCameraMod->setDisabled(true);
	m_ui.actionCameraDel->setDisabled(true);
	// ������ര���Ҽ��˵�ģʽ => �Զ���ģʽ...
	m_ui.LeftView->setContextMenuPolicy(Qt::CustomContextMenu);
	// ����ͨ���˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraAdd(bool)), m_ui.actionCameraAdd, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraMod(bool)), m_ui.actionCameraMod, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraDel(bool)), m_ui.actionCameraDel, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(on_LeftViewCustomContextMenuRequested(const QPoint&)));
	// �������ò˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionSettingReconnect);
	m_ui.mainToolBar->addAction(m_ui.actionSettingSystem);
	m_ui.mainToolBar->addSeparator();
	// ���öϿ������˵���ťΪ������...
	m_ui.actionSettingReconnect->setDisabled(true);
	m_ui.actionSettingSystem->setDisabled(true);
	// �������ò˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enableSettingReconnect(bool)), m_ui.actionSettingReconnect, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableSettingSystem(bool)), m_ui.actionSettingSystem, SLOT(setEnabled(bool)));
	// ���ù��ڲ˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionHelpAbout);
	// ���ò˵�����ʾ�����źŲ��¼�...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraPreview(bool)), m_ui.actionCameraPreview, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePreviewMute(bool)), m_ui.actionPreviewMute, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraPTZ(bool)), m_ui.actionCameraPTZ, SLOT(setEnabled(bool)));
	this->connect(m_ui.cameraMenu, SIGNAL(aboutToShow()), this, SLOT(onCameraMenuToShow()));
	// �������Ҵ��ڵĴ�С���� => 20:80...
	m_ui.splitter->setContentsMargins(0, 0, 0, 0);
	m_ui.splitter->setStretchFactor(0, 20);
	m_ui.splitter->setStretchFactor(1, 80);
	// ���������ڱ�����...
	this->UpdateTitleBar();
	// �ָ���������λ��...
	const char *geometry = config_get_string(App()->GlobalConfig(), "BasicWindow", "geometry");
	if (geometry != NULL) {
		QByteArray byteArray = QByteArray::fromBase64(QByteArray(geometry));
		this->restoreGeometry(byteArray);
		QRect windowGeometry = this->normalGeometry();
		if (!WindowPositionValid(windowGeometry)) {
			const QRect & rect = App()->desktop()->geometry();
			setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,size(), rect));
		}
	}
}

// ��Ӧ����ͷͨ���˵���ʾ�����¼�...
void StudentWindow::onCameraMenuToShow()
{
	CViewCamera * lpViewCamera = NULL;
	int nFocusID = m_ui.LeftView->GetFocusID();
	lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	// �����ǰ���㴰������������ͷͨ��...
	if (lpViewCamera != NULL) {
		bool bIsOffLine = lpViewCamera->IsCameraOffLine();
		m_ui.actionCameraStart->setEnabled(bIsOffLine);
		m_ui.actionCameraStop->setEnabled(!bIsOffLine);
		m_ui.actionCameraMod->setEnabled(true);
		m_ui.actionCameraDel->setEnabled(true);
		// �������ͷͨ����Ԥ����PTZ�����������˵�...
		bool bIsPreviewShow = lpViewCamera->IsCameraPreviewShow();
		m_ui.actionCameraPreview->setEnabled(true);
		m_ui.actionCameraPTZ->setEnabled(true);
		m_ui.actionCameraPreview->setCheckable(true);
		m_ui.actionCameraPreview->setChecked(bIsPreviewShow);
		// �����˵��Ƿ���ЧҪ����Ԥ���˵�...
		bool bIsPreviewMute = lpViewCamera->IsCameraPreviewMute();
		m_ui.actionPreviewMute->setCheckable(true);
		m_ui.actionPreviewMute->setChecked(bIsPreviewMute);
		m_ui.actionPreviewMute->setEnabled(bIsPreviewShow);
	} else {
		m_ui.actionCameraStart->setEnabled(false);
		m_ui.actionCameraStop->setEnabled(false);
		m_ui.actionCameraMod->setEnabled(false);
		m_ui.actionCameraDel->setEnabled(false);
		// �������ͷͨ����Ԥ����PTZ�����������˵�...
		m_ui.actionCameraPreview->setEnabled(false);
		m_ui.actionPreviewMute->setEnabled(false);
		m_ui.actionCameraPTZ->setEnabled(false);
	}
}

// ��Ӧ��ര�ڵ��Ҽ��˵��¼�...
void StudentWindow::on_LeftViewCustomContextMenuRequested(const QPoint &pos)
{
	// ��ȡ�Ҽ�λ�õ�����ͷ���ڶ��������Ƶ�طŴ��ڶ���...
	CViewRender * lpViewPlayer = qobject_cast<CViewRender*>(m_ui.LeftView->childAt(pos));
	CViewCamera * lpViewCamera = qobject_cast<CViewCamera*>(m_ui.LeftView->childAt(pos));
	// �����Ƶ�طŴ�����Ч����ȡ�����ڵ�����ͷ����...
	if (lpViewPlayer != NULL) {
		lpViewCamera = qobject_cast<CViewCamera*>(lpViewPlayer->parent());
	}
	// �������ͷ���ڶ�����Ч��ֱ�ӷ���...
	if (lpViewCamera == NULL)
		return;
	// ���ѡ������ͷ���ڲ��ǽ��㴰�ڣ��򽹵㴰�ڵı��������ͷ����ı�Ų�һ�£�ֱ�ӷ���...
	if (!lpViewCamera->IsFoucs() || m_ui.LeftView->GetFocusID() != lpViewCamera->GetDBCameraID())
		return;
	// ��������ͷ���ڵĵ�ǰ״̬�����Ҽ��˵��������...
	QMenu popup(this);
	bool bIsPreviewShow = lpViewCamera->IsCameraPreviewShow();
	bool bIsPreviewMute = lpViewCamera->IsCameraPreviewMute();
	m_ui.actionCameraPreview->setCheckable(true);
	m_ui.actionCameraPreview->setChecked(bIsPreviewShow);
	// �����˵��Ƿ���ЧҪ����Ԥ���˵�...
	m_ui.actionPreviewMute->setCheckable(true);
	m_ui.actionPreviewMute->setChecked(bIsPreviewMute);
	m_ui.actionPreviewMute->setEnabled(bIsPreviewShow);
	popup.addAction(m_ui.actionCameraPreview);
	popup.addAction(m_ui.actionPreviewMute);
	popup.addAction(m_ui.actionCameraPTZ);
	popup.exec(QCursor::pos());
}

// ��Ӧ����ͷͨ���Ļ���Ԥ���˵��¼�...
void StudentWindow::on_actionCameraPreview_triggered()
{
	// ������ര�ڰ����ĵ�ǰ��������ͷ���ڶ���...
	int nFocusID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	if (lpViewCamera == NULL)
		return;
	// ִ������ͷ���ڵ�Ԥ���¼�...
	lpViewCamera->doTogglePreviewShow();
}

// ��Ӧ����ͷͨ����Ԥ�������˵��¼�...
void StudentWindow::on_actionPreviewMute_triggered()
{
	// ������ര�ڰ����ĵ�ǰ��������ͷ���ڶ���...
	int nFocusID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	if (lpViewCamera == NULL)
		return;
	// ִ������ͷ���ڵ�Ԥ���¼�...
	lpViewCamera->doTogglePreviewMute();
}

// ��Ӧ����ͷͨ����PTZ��̨�˵��¼�...
void StudentWindow::on_actionCameraPTZ_triggered()
{
}

// ��Ӧ���������͵�UDP���Ӷ���ɾ�����¼�֪ͨ...
void StudentWindow::onTriggerUdpLogout(int tmTag, int idTag, int nDBCameraID)
{
	// �������ѧ���˶��� => ֱ�ӷ���...
	if (tmTag != TM_TAG_STUDENT)
		return;
	// �����ѧ���ۿ��˵�ɾ��֪ͨ => ʹ���Ҳര�ڶ���...
	if (idTag == ID_TAG_LOOKER) {
		CViewTeacher * lpViewTeacher = m_ui.RightView->GetViewTeacher();
		((lpViewTeacher != NULL) ? lpViewTeacher->onTriggerUdpRecvThread(false) : NULL);
		return;
	}
	// �����ѧ�������˵�ɾ��֪ͨ => ʹ����ര�ڶ���...
	if (idTag == ID_TAG_PUSHER) {
		m_ui.LeftView->doUdpPusherLogout(nDBCameraID);
		return;
	}
}

void StudentWindow::doWebThreadMsg(int nMessageID, int nWParam, int nLParam)
{
	if (nMessageID == WM_WEB_AUTH_RESULT) {
		m_ui.RightView->onWebAuthResult(nWParam, (bool)nLParam);
		if ((nWParam == kAuthExpired) && (nLParam <= 0)) {
			m_ui.LeftView->onWebAuthExpired();
			App()->onWebAuthExpired();
		}
	}
	if (nMessageID == WM_WEB_LOAD_RESOURCE) {
		m_ui.RightView->onWebLoadResource();
		m_ui.LeftView->onWebLoadResource();
		App()->onWebLoadResource();
	}
}

StudentWindow::~StudentWindow()
{
	// ���ڹر�ǰ���������е�������Ϣ...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// �Դ��ڱ�������޸� => ʹ���ֵ�ģʽ...
	string & strRoomID = App()->GetRoomIDStr();
	QString strTitle = QString("%1%2").arg(QTStr("Main.Window.TitleContent")).arg(QString::fromUtf8(strRoomID.c_str()));
	this->setWindowTitle(strTitle);
}

void StudentWindow::InitWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);

	this->show();

	if (windowState().testFlag(Qt::WindowFullScreen)) {
		this->showNormal();
	}
}

void StudentWindow::closeEvent(QCloseEvent *event)
{
	// �����˳�ѯ�ʿ�...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmExit.Title"),
		QTStr("ConfirmExit.Quit"));
	if (button == QMessageBox::No) {
		event->ignore();
		return;
	}
	// �����ڵ����걣������...
	if (this->isVisible()) {
		config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry",	saveGeometry().toBase64().constData());
	}
	// ִ��close�¼�...
	QWidget::closeEvent(event);
	if (!event->isAccepted())
		return;
	// ��ӡ�ر���־...
	blog(LOG_INFO, SHUTDOWN_SEPARATOR);
	// �����˳��¼�֪ͨ...
	App()->doLogoutEvent();
	// ���ùر��˳��ӿ�...
	App()->quit();
}

void StudentWindow::on_actionSystemToolbar_triggered()
{
	bool bIsVisible = m_ui.mainToolBar->isVisible();
	m_ui.actionSystemToolbar->setCheckable(true);
	m_ui.actionSystemToolbar->setChecked(!bIsVisible);
	m_ui.mainToolBar->setVisible(!bIsVisible);
}

void StudentWindow::on_actionSystemExit_triggered()
{
	this->close();
}

void StudentWindow::on_actionSystemFullscreen_triggered()
{
	//m_ui.actionSystemFullscreen->setCheckable(true);
	//m_ui.actionSystemFullscreen->setChecked(true);
	m_ui.RightView->onFullScreenAction();
}

void StudentWindow::on_actionSettingReconnect_triggered()
{
	// ����ȷ�ϣ��Ƿ�Ҫ������������...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("Confirm.Reconnect.Title"),
		QTStr("Confirm.Reconnect.Text"));
	if (button == QMessageBox::No)
		return;
	// ɾ�������Դ��ɾ���Ҳ���Դ...
	m_ui.LeftView->doDestoryResource();
	m_ui.RightView->doDestroyResource();
	// �˳����ؽ�ϵͳ��Դ�������µ���վ�߳�...
	App()->doReBuildResource();
}

void StudentWindow::on_actionSettingSystem_triggered()
{
}

void StudentWindow::on_actionCameraAdd_triggered()
{
	// ������ͷ�������� => ���...
	CDlgPush dlg(this, -1, false);
	if (dlg.exec() == QDialog::Rejected)
		return;
	// ���ȷ��֮��Ĵ������...
	CViewCamera * lpViewCamera = NULL;
	CWebThread * lpWebThread = App()->GetWebThread();
	GM_MapData & theMapData = dlg.GetPushData();
	// ���ڷ�������ע��ͨ����Ȼ���ڱ��ش�������ͷ���ڶ��������µ�����...
	((lpWebThread != NULL) ? lpWebThread->doWebRegCamera(theMapData) : NULL);
	// ��Ҫ�������½�������ͷ������Ҫ�������У����ҵ�����ҳ�˵�...
	lpViewCamera = m_ui.LeftView->AddNewCamera(theMapData);
	((lpViewCamera != NULL) ? lpViewCamera->update() : NULL);
	// ֱ������ͨ�� => ͨ�����Ľ��㴰��...
	m_ui.LeftView->onCameraStart();
}

void StudentWindow::on_actionCameraMod_triggered()
{
	// ������ͷ�������� => �޸�...
	CViewCamera * lpViewCamera = NULL;
	int nDBCameraID = m_ui.LeftView->GetFocusID();
	lpViewCamera = m_ui.LeftView->FindDBCameraByID(nDBCameraID);
	if (nDBCameraID <= 0 || lpViewCamera == NULL)
		return;
	// �����Զ����� => �޸�ʱ�������Զ�����...
	m_ui.LeftView->SetCanAutoLink(false);
	// ����ͨ�������޸Ŀ� => ע��ָ��Զ�������־...
	CDlgPush dlg(this, nDBCameraID, lpViewCamera->IsCameraOffLine());
	if (dlg.exec() == QDialog::Rejected) {
		m_ui.LeftView->SetCanAutoLink(true);
		return;
	}
	// ���ȷ��֮��Ĵ������...
	CWebThread * lpWebThread = App()->GetWebThread();
	GM_MapData & theMapData = dlg.GetPushData();
	// ���ڷ������ϸ���ͨ����Ȼ�󽫽�����µ�����...
	((lpWebThread != NULL) ? lpWebThread->doWebRegCamera(theMapData) : NULL);
	((lpViewCamera != NULL) ? lpViewCamera->update() : NULL);
	// ֱ������ͨ�� => ͨ�����Ľ��㴰��...
	m_ui.LeftView->onCameraStart();
	// �ָ���ര�ڵ��Զ�������־...
	m_ui.LeftView->SetCanAutoLink(true);
}

void StudentWindow::on_actionCameraDel_triggered()
{
	// ȷ�ϵ�ǰѡ�н����Ƿ���Ч...
	int nDBCameraID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nDBCameraID);
	if (lpViewCamera == NULL)
		return;
	CWebThread * lpWebThread = App()->GetWebThread();
	string strDeviceSN = App()->GetCameraDeviceSN(nDBCameraID);
	if (lpWebThread == NULL || strDeviceSN.size() <= 0)
		return;
	// ɾ��ȷ�� => ��ʾ��ʾ��Ϣ...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmDel.Title"), QTStr("ConfirmDel.Text"));
	if (button == QMessageBox::No)
		return;
	// ����ת�������㱨ͨ����Ϣ��״̬...
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	if (lpRemoteSession != NULL) {
		lpRemoteSession->doSendStopCameraCmd(nDBCameraID);
	}
	// ��ര�ڷ���ɾ������...
	m_ui.LeftView->onCameraDel(nDBCameraID);
	// ֪ͨ��վ����ɾ������...
	lpWebThread->doWebDelCamera(strDeviceSN);
	// ȫ�����÷���ɾ������...
	App()->doDelCamera(nDBCameraID);
}

void StudentWindow::on_actionCameraStart_triggered()
{
	m_ui.LeftView->onCameraStart();
}

void StudentWindow::on_actionCameraStop_triggered()
{
	m_ui.LeftView->onCameraStop();
}

void StudentWindow::on_actionPagePrev_triggered()
{
	m_ui.LeftView->onPagePrev();
}

void StudentWindow::on_actionPageJump_triggered()
{
	m_ui.LeftView->onPageJump();
}

void StudentWindow::on_actionPageNext_triggered()
{
	m_ui.LeftView->onPageNext();
}

void StudentWindow::on_actionHelpAbout_triggered()
{
}
