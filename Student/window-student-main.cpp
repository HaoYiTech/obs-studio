
#include <QMessageBox>
#include <QShowEvent>
#include <QDesktopWidget>
#include <QRect>
#include <QScreen>

#include "HYDefine.h"
#include "web-thread.h"
#include "student-app.h"
#include "win-update.hpp"
#include "qt-wrappers.hpp"
#include "window-dlg-push.hpp"
#include "window-dlg-about.hpp"
#include "window-dlg-setsys.hpp"
#include "window-student-main.h"
#include "window-view-camera.hpp"
#include "window-view-render.hpp"
#include "window-view-teacher.hpp"
#include "window-view-ptz.h"

#define STARTUP_SEPARATOR   "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR 	"==== Shutting down =================================================="

#define UPDATE_CHECK_INTERVAL (60*60*24*4) /* 4 days */

StudentWindow::StudentWindow(QWidget *parent)
 : QMainWindow(parent)
{
	// 设置界面对象...
	m_ui.setupUi(this);
	// 设置窗口图标 => 必须用png图片 => 解决有些机器不认ico，造成左上角图标无法显示...
	this->setWindowIcon(QIcon(":/res/images/student.png"));
	// 工具栏属于显示状态...
	m_ui.actionSystemToolbar->setCheckable(true);
	m_ui.actionSystemToolbar->setChecked(true);
	// 关联工具栏菜单事件响应...
	m_ui.mainToolBar->addAction(m_ui.actionPagePrev);
	m_ui.mainToolBar->addAction(m_ui.actionPageJump);
	m_ui.mainToolBar->addAction(m_ui.actionPageNext);
	m_ui.mainToolBar->addSeparator();
	// 设置翻页菜单按钮为不可用...
	m_ui.actionPagePrev->setDisabled(true);
	m_ui.actionPageJump->setDisabled(true);
	m_ui.actionPageNext->setDisabled(true);
	// 设置工具栏、菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enablePagePrev(bool)), m_ui.actionPagePrev, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePageJump(bool)), m_ui.actionPageJump, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePageNext(bool)), m_ui.actionPageNext, SLOT(setEnabled(bool)));
	// 设置全屏菜单按钮...
	m_ui.mainToolBar->addAction(m_ui.actionSystemFullscreen);
	m_ui.mainToolBar->addSeparator();
	// 设置通道操作菜单按钮不可用...
	m_ui.actionSystemFullscreen->setDisabled(true);
	// 设置全屏操作菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enableSystemFullscreen(bool)), m_ui.actionSystemFullscreen, SLOT(setEnabled(bool)));
	// 设置通道操作菜单按钮...
	m_ui.mainToolBar->addAction(m_ui.actionCameraStart);
	m_ui.mainToolBar->addAction(m_ui.actionCameraStop);
	m_ui.mainToolBar->addSeparator();
	// 设置通道操作菜单按钮不可用...
	m_ui.actionCameraStart->setDisabled(true);
	m_ui.actionCameraStop->setDisabled(true);
	// 设置通道操作菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraStart(bool)), m_ui.actionCameraStart, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraStop(bool)), m_ui.actionCameraStop, SLOT(setEnabled(bool)));
	// 设置通道菜单按钮...
	m_ui.mainToolBar->addAction(m_ui.actionCameraAdd);
	m_ui.mainToolBar->addAction(m_ui.actionCameraMod);
	m_ui.mainToolBar->addAction(m_ui.actionCameraDel);
	m_ui.mainToolBar->addSeparator();
	// 设置通道菜单按钮为不可用...
	m_ui.actionCameraAdd->setDisabled(true);
	m_ui.actionCameraMod->setDisabled(true);
	m_ui.actionCameraDel->setDisabled(true);
	// 设置左侧窗口右键菜单模式 => 自定义模式...
	m_ui.LeftView->setContextMenuPolicy(Qt::CustomContextMenu);
	// 设置通道菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraAdd(bool)), m_ui.actionCameraAdd, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraMod(bool)), m_ui.actionCameraMod, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraDel(bool)), m_ui.actionCameraDel, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(on_LeftViewCustomContextMenuRequested(const QPoint&)));
	// 设置配置菜单按钮...
	m_ui.mainToolBar->addAction(m_ui.actionSettingReconnect);
	m_ui.mainToolBar->addAction(m_ui.actionSettingSystem);
	m_ui.mainToolBar->addSeparator();
	// 设置断开重连菜单按钮为不可用...
	m_ui.actionSettingReconnect->setDisabled(true);
	m_ui.actionSettingSystem->setDisabled(true);
	// 设置配置菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enableSettingReconnect(bool)), m_ui.actionSettingReconnect, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableSettingSystem(bool)), m_ui.actionSettingSystem, SLOT(setEnabled(bool)));
	// 设置关于菜单按钮...
	m_ui.mainToolBar->addAction(m_ui.actionHelpAbout);
	// 设置菜单栏显示关联信号槽事件...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraPreview(bool)), m_ui.actionCameraPreview, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enablePreviewMute(bool)), m_ui.actionPreviewMute, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraPTZ(bool)), m_ui.actionCameraPTZ, SLOT(setEnabled(bool)));
	this->connect(m_ui.cameraMenu, SIGNAL(aboutToShow()), this, SLOT(onCameraMenuToShow()));
	// 设置左右窗口的大小比例 => 20:80...
	m_ui.splitter->setContentsMargins(0, 0, 0, 0);
	m_ui.splitter->setStretchFactor(0, 20);
	m_ui.splitter->setStretchFactor(1, 80);
	// 更新主窗口标题栏...
	this->UpdateTitleBar();
	// 恢复窗口坐标位置...
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
	// 创建云台控制窗口，并隐藏起来...
	m_PTZWindow = new CPTZWindow(this);
	m_PTZWindow->hide();
}

// 响应摄像头通道菜单显示激活事件...
void StudentWindow::onCameraMenuToShow()
{
	CViewCamera * lpViewCamera = NULL;
	int nFocusID = m_ui.LeftView->GetFocusID();
	lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	// 如果当前焦点窗口是左侧的摄像头通道...
	if (lpViewCamera != NULL) {
		bool bIsOffLine = lpViewCamera->IsCameraOffLine();
		m_ui.actionCameraStart->setEnabled(bIsOffLine);
		m_ui.actionCameraStop->setEnabled(!bIsOffLine);
		m_ui.actionCameraMod->setEnabled(true);
		m_ui.actionCameraDel->setEnabled(true);
		// 针对摄像头通道的预览、PTZ、静音操作菜单...
		bool bIsPreviewShow = lpViewCamera->IsCameraPreviewShow();
		m_ui.actionCameraPreview->setEnabled(true);
		m_ui.actionCameraPTZ->setEnabled(true);
		m_ui.actionCameraPreview->setCheckable(true);
		m_ui.actionCameraPreview->setChecked(bIsPreviewShow);
		// 静音菜单是否有效要依赖预览菜单...
		bool bIsPreviewMute = lpViewCamera->IsCameraPreviewMute();
		m_ui.actionPreviewMute->setCheckable(true);
		m_ui.actionPreviewMute->setChecked(bIsPreviewMute);
		m_ui.actionPreviewMute->setEnabled(bIsPreviewShow);
	} else {
		m_ui.actionCameraStart->setEnabled(false);
		m_ui.actionCameraStop->setEnabled(false);
		m_ui.actionCameraMod->setEnabled(false);
		m_ui.actionCameraDel->setEnabled(false);
		// 针对摄像头通道的预览、PTZ、静音操作菜单...
		m_ui.actionCameraPreview->setEnabled(false);
		m_ui.actionPreviewMute->setEnabled(false);
		m_ui.actionCameraPTZ->setEnabled(false);
	}
}

// 响应系统层的键盘事件 => 音量的放大或缩小事件...
bool StudentWindow::VolumeEvent(QObject * inObject, int inKeyItem)
{
	// 过滤键盘按键，不是加号或减号(包括大小键盘)，返回失败...
	if (inKeyItem != Qt::Key_Less && inKeyItem != Qt::Key_Equal && inKeyItem != Qt::Key_Minus && inKeyItem != Qt::Key_Plus)
		return false;
	// 先让左侧视图进行键盘事件处理...
	if (m_ui.LeftView->doVolumeEvent(inKeyItem))
		return true;
	// 处理失败，再获取右侧讲师窗口，无效，直接返回失败...
	CViewTeacher * lpViewTeacher = m_ui.RightView->GetViewTeacher();
	if (lpViewTeacher == NULL)
		return false;
	// 再让讲师窗口进行键盘事件处理...
	return lpViewTeacher->doVolumeEvent(inKeyItem);
}

// 响应左侧窗口的右键菜单事件...
void StudentWindow::on_LeftViewCustomContextMenuRequested(const QPoint &pos)
{
	// 获取右键位置的摄像头窗口对象或者视频回放窗口对象...
	CViewRender * lpViewPlayer = qobject_cast<CViewRender*>(m_ui.LeftView->childAt(pos));
	CViewCamera * lpViewCamera = qobject_cast<CViewCamera*>(m_ui.LeftView->childAt(pos));
	// 如果视频回放窗口有效，读取父窗口的摄像头对象...
	if (lpViewPlayer != NULL) {
		lpViewCamera = qobject_cast<CViewCamera*>(lpViewPlayer->parent());
	}
	// 如果摄像头窗口对象无效，直接返回...
	if (lpViewCamera == NULL)
		return;
	// 如果选中摄像头窗口不是焦点窗口，或焦点窗口的编号与摄像头里面的编号不一致，直接返回...
	if (!lpViewCamera->IsFoucs() || m_ui.LeftView->GetFocusID() != lpViewCamera->GetDBCameraID())
		return;
	// 根据摄像头窗口的当前状态进行右键菜单项的配置...
	QMenu popup(this);
	bool bIsPreviewShow = lpViewCamera->IsCameraPreviewShow();
	bool bIsPreviewMute = lpViewCamera->IsCameraPreviewMute();
	bool bIsCameraOnLine = lpViewCamera->IsCameraOnLine();
	bool bIsLoginISAPI = lpViewCamera->IsCameraLoginISAPI();
	// 设定云台操作菜单的有效性...
	//m_ui.actionCameraPTZ->setEnabled(bIsLoginISAPI);
	// 设置画面预览菜单的有效性...
	m_ui.actionCameraPreview->setCheckable(true);
	m_ui.actionCameraPreview->setChecked(bIsPreviewShow);
	m_ui.actionCameraPreview->setEnabled(bIsCameraOnLine);
	// 静音菜单是否有效要依赖预览菜单...
	m_ui.actionPreviewMute->setCheckable(true);
	m_ui.actionPreviewMute->setChecked(bIsPreviewMute);
	m_ui.actionPreviewMute->setEnabled(bIsPreviewShow);
	// 加入菜单列表，显示右键菜单...
	popup.addAction(m_ui.actionCameraPreview);
	popup.addAction(m_ui.actionPreviewMute);
	popup.addAction(m_ui.actionCameraPTZ);
	popup.exec(QCursor::pos());
}

// 响应摄像头通道的画面预览菜单事件...
void StudentWindow::on_actionCameraPreview_triggered()
{
	// 查找左侧窗口包含的当前焦点摄像头窗口对象...
	int nFocusID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	if (lpViewCamera == NULL)
		return;
	// 执行摄像头窗口的预览事件...
	lpViewCamera->doTogglePreviewShow();
}

// 响应摄像头通道的预览静音菜单事件...
void StudentWindow::on_actionPreviewMute_triggered()
{
	// 查找左侧窗口包含的当前焦点摄像头窗口对象...
	int nFocusID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nFocusID);
	if (lpViewCamera == NULL)
		return;
	// 执行摄像头窗口的预览事件...
	lpViewCamera->doTogglePreviewMute();
}

// 响应摄像头通道的PTZ云台菜单事件...
void StudentWindow::on_actionCameraPTZ_triggered()
{
	if (m_PTZWindow != NULL) {
		m_PTZWindow->doUpdatePTZ(m_ui.LeftView->GetFocusID());
		m_PTZWindow->show();
	}
}

// 焦点窗口变化时，更新PTZ云台...
void StudentWindow::doUpdatePTZ(int nDBCameraID)
{
	if (m_PTZWindow != NULL) {
		m_PTZWindow->doUpdatePTZ(nDBCameraID);
	}
}

// 响应服务器发送的UDP连接对象被删除的事件通知...
void StudentWindow::onTriggerUdpLogout(int tmTag, int idTag, int nDBCameraID)
{
	// 如果不是学生端对象 => 直接返回...
	if (tmTag != TM_TAG_STUDENT)
		return;
	// 如果是学生观看端的删除通知 => 使用右侧窗口对象...
	if (idTag == ID_TAG_LOOKER) {
		CViewTeacher * lpViewTeacher = m_ui.RightView->GetViewTeacher();
		((lpViewTeacher != NULL) ? lpViewTeacher->onTriggerUdpRecvThread(false) : NULL);
		return;
	}
	// 如果是学生推流端的删除通知 => 使用左侧窗口对象...
	if (idTag == ID_TAG_PUSHER) {
		m_ui.LeftView->doUdpPusherLogout(nDBCameraID);
		return;
	}
}

// 响应讲师端远程发送的云台控制命令 => 找到指定的摄像头通道，并发起云台控制命令...
void StudentWindow::onTriggerCameraPTZ(int nDBCameraID, int nCmdID, int nSpeedVal)
{
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nDBCameraID);
	((lpViewCamera != NULL) ? lpViewCamera->doPTZCmd((CMD_ISAPI)nCmdID, nSpeedVal) : NULL);
}

void StudentWindow::doWebThreadMsg(int nMessageID, int nWParam, int nLParam)
{
	if (nMessageID == WM_WEB_AUTH_RESULT) {
		m_ui.RightView->onWebAuthResult(nWParam, (bool)nLParam);
		if ((nWParam == kAuthExpired) && (nLParam <= 0)) {
			m_ui.LeftView->onWebAuthExpired();
			App()->onWebAuthExpired();
		}
		// 更新标题栏名称内容...
		this->UpdateTitleBar();
	}
	if (nMessageID == WM_WEB_LOAD_RESOURCE) {
		m_ui.RightView->onWebLoadResource();
		m_ui.LeftView->onWebLoadResource();
		App()->onWebLoadResource();
	}
}

StudentWindow::~StudentWindow()
{
	// 等待并删除自动升级线程的退出...
	if (updateCheckThread && updateCheckThread->isRunning()) {
		updateCheckThread->wait();
		delete updateCheckThread;
	}
	// 窗口关闭前，保存所有的配置信息...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// 对窗口标题进行修改 => 使用字典模式...
	string & strRoomID = App()->GetRoomIDStr();
	QString strRole = App()->GetRoleString();
	QString strTitle = QString("%1%2 - %3").arg(QTStr("Main.Window.TitleContent")).arg(QString::fromUtf8(strRoomID.c_str())).arg(strRole);
	this->setWindowTitle(strTitle);
}

void StudentWindow::TimedCheckForUpdates()
{
	// 检测是否开启自动更新开关，默认是处于开启状态 => CStudentApp::InitGlobalConfigDefaults()...
	if (!config_get_bool(App()->GlobalConfig(), "General", "EnableAutoUpdates"))
		return;
	// 注意：LastUpdateCheck只在用户点击“稍后提醒”才会起作用(4天后)；如果是相同版本，也会每次启动都会检测；
	long long lastUpdate = config_get_int(App()->GlobalConfig(), "General", "LastUpdateCheck");
	uint32_t lastVersion = config_get_int(App()->GlobalConfig(), "General", "LastVersion");
	// 如果上次升级版本比当前exe存放版本还要小，立即升级...
	if (lastVersion < LIBOBS_API_VER) {
		lastUpdate = 0;
		config_set_int(App()->GlobalConfig(), "General", "LastUpdateCheck", 0);
	}
	// 计算当前时间与上次升级之间的时间差...
	long long t = (long long)time(nullptr);
	long long secs = t - lastUpdate;
	// 时间差超过4天，开始检查并执行升级...
	if (secs > UPDATE_CHECK_INTERVAL) {
		this->CheckForUpdates(false);
	}
}

void StudentWindow::CheckForUpdates(bool manualUpdate)
{
	// 屏蔽升级操作菜单，避免在升级过程中重复升级...
	m_ui.actionSettingUpdater->setEnabled(false);
	if (updateCheckThread && updateCheckThread->isRunning())
		return;
	// 创建升级线程，并启动之 => 可以手动直接升级...
	updateCheckThread = new AutoUpdateThread(manualUpdate);
	updateCheckThread->start();

	UNUSED_PARAMETER(manualUpdate);
}

void StudentWindow::updateCheckFinished()
{
	m_ui.actionSettingUpdater->setEnabled(true);
}

void StudentWindow::InitWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);

	this->show();

	if (windowState().testFlag(Qt::WindowFullScreen)) {
		this->showNormal();
	}

	// 窗口初始化完毕，进行升级检测...
	this->TimedCheckForUpdates();
}

void StudentWindow::closeEvent(QCloseEvent *event)
{
	QMessageBox::StandardButton button;
	// 如果不是沉默退出，需要弹出退出询问框...
	if (!this->m_bIsSlientClose) {
		button = OBSMessageBox::question(
			this, QTStr("ConfirmExit.Title"),
			QTStr("ConfirmExit.Quit"));
		if (button == QMessageBox::No) {
			event->ignore();
			return;
		}
	}
	// 将窗口的坐标保存起来...
	if (this->isVisible()) {
		config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry",	saveGeometry().toBase64().constData());
	}
	// 执行close事件...
	QWidget::closeEvent(event);
	if (!event->isAccepted())
		return;
	// 打印关闭日志...
	blog(LOG_INFO, SHUTDOWN_SEPARATOR);
	// 等待自动检查升级线程的退出...
	if (updateCheckThread != NULL) {
		updateCheckThread->wait();
	}
	// 调用退出事件通知...
	App()->doLogoutEvent();
	// 调用关闭退出接口...
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
	// 弹框确认，是否要进行重连操作...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("Confirm.Reconnect.Title"),
		QTStr("Confirm.Reconnect.Text"));
	if (button == QMessageBox::No)
		return;
	// 删除左侧资源，删除右侧资源...
	m_ui.LeftView->doDestoryResource();
	m_ui.RightView->doDestroyResource();
	// 退出并重建系统资源，构建新的网站线程...
	App()->doReBuildResource();
}

void StudentWindow::on_actionSettingUpdater_triggered()
{
	this->CheckForUpdates(true);
}

void StudentWindow::on_actionSettingSystem_triggered()
{
	CDlgSetSys dlg(this);
	ROLE_TYPE  nOldRole = App()->GetRoleType();
	if (dlg.exec() == QDialog::Rejected)
		return;
	// 如果学生端的身份角色发生变化，需要断开重连...
	ROLE_TYPE  nNewRole = App()->GetRoleType();
	if (nOldRole != nNewRole) {
		// 删除左侧资源，删除右侧资源...
		m_ui.LeftView->doDestoryResource();
		m_ui.RightView->doDestroyResource();
		// 退出并重建系统资源，构建新的网站线程...
		App()->doReBuildResource();
		return;
	}
	// 保存成功，修改组播对象的网络发送接口...
	CViewRight * lpViewRight = this->GetViewRight();
	CViewTeacher * lpViewTeacher = lpViewRight->GetViewTeacher();
	(lpViewTeacher != NULL) ? lpViewTeacher->doResetMulticastIPSend() : NULL;
}

void StudentWindow::on_actionCameraAdd_triggered()
{
	// 打开摄像头窗口配置 => 添加...
	CDlgPush dlg(this, -1);
	if (dlg.exec() == QDialog::Rejected)
		return;
	// 点击确定之后的处理过程...
	CViewCamera * lpViewCamera = NULL;
	CWebThread * lpWebThread = App()->GetWebThread();
	GM_MapData & theMapData = dlg.GetPushData();
	// 先在服务器上注册通道，然后在本地创建摄像头窗口对象，最后更新到界面...
	((lpWebThread != NULL) ? lpWebThread->doWebRegCamera(theMapData) : NULL);
	// 需要单独对新建的摄像头窗口需要重新排列，并且调整翻页菜单...
	lpViewCamera = m_ui.LeftView->AddNewCamera(theMapData);
	((lpViewCamera != NULL) ? lpViewCamera->update() : NULL);
	// 直接启动通道 => 通过左侧的焦点窗口...
	m_ui.LeftView->onCameraStart();
}

void StudentWindow::on_actionCameraMod_triggered()
{
	// 打开摄像头窗口配置 => 修改...
	CViewCamera * lpViewCamera = NULL;
	int nDBCameraID = m_ui.LeftView->GetFocusID();
	lpViewCamera = m_ui.LeftView->FindDBCameraByID(nDBCameraID);
	if (nDBCameraID <= 0 || lpViewCamera == NULL)
		return;
	// 屏蔽自动重连 => 修改时，不能自动重连...
	m_ui.LeftView->SetCanAutoLink(false);
	// 弹出通道配置修改框 => 注意恢复自动重连标志...
	CDlgPush dlg(this, nDBCameraID);
	if (dlg.exec() == QDialog::Rejected) {
		m_ui.LeftView->SetCanAutoLink(true);
		return;
	}
	// 如果通道处于已连接状态，先进行停止操作...
	if (lpViewCamera->IsCameraOnLine()) {
		m_ui.LeftView->onCameraStop();
	}
	// 点击确定之后的处理过程...
	CWebThread * lpWebThread = App()->GetWebThread();
	GM_MapData & theMapData = dlg.GetPushData();
	// 先在服务器上更新通道，然后将结果更新到界面...
	((lpWebThread != NULL) ? lpWebThread->doWebRegCamera(theMapData) : NULL);
	((lpViewCamera != NULL) ? lpViewCamera->update() : NULL);
	// 直接启动通道 => 通过左侧的焦点窗口...
	m_ui.LeftView->onCameraStart();
	// 恢复左侧窗口的自动重连标志...
	m_ui.LeftView->SetCanAutoLink(true);
}

void StudentWindow::on_actionCameraDel_triggered()
{
	// 确认当前选中焦点是否有效...
	int nDBCameraID = m_ui.LeftView->GetFocusID();
	CViewCamera * lpViewCamera = m_ui.LeftView->FindDBCameraByID(nDBCameraID);
	if (lpViewCamera == NULL)
		return;
	CWebThread * lpWebThread = App()->GetWebThread();
	string strDeviceSN = App()->GetCameraDeviceSN(nDBCameraID);
	if (lpWebThread == NULL || strDeviceSN.size() <= 0)
		return;
	// 删除确认 => 显示提示信息...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmDel.Title"), QTStr("ConfirmDel.Text"));
	if (button == QMessageBox::No)
		return;
	// 向中转服务器汇报通道信息和状态...
	CRemoteSession * lpRemoteSession = App()->GetRemoteSession();
	if (lpRemoteSession != NULL) {
		lpRemoteSession->doSendStopCameraCmd(nDBCameraID);
	}
	// 左侧窗口发起删除操作...
	m_ui.LeftView->onCameraDel(nDBCameraID);
	// 通知网站发起删除操作...
	lpWebThread->doWebDelCamera(strDeviceSN);
	// 全局配置发起删除操作...
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
	CDlgAbout dlg(this);
	dlg.exec();
}
