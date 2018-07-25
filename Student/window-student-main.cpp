
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

#define STARTUP_SEPARATOR   "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR 	"==== Shutting down =================================================="

StudentWindow::StudentWindow(QWidget *parent)
 : QMainWindow(parent)
{
	// 设置界面对象...
	m_ui.setupUi(this);
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
	// 设置通道菜单按钮信号槽...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraAdd(bool)), m_ui.actionCameraAdd, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraMod(bool)), m_ui.actionCameraMod, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraDel(bool)), m_ui.actionCameraDel, SLOT(setEnabled(bool)));
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
}

void StudentWindow::doWebThreadMsg(int nMessageID, int nWParam, int nLParam)
{
	if (nMessageID == WM_WEB_AUTH_RESULT) {
		m_ui.RightView->onWebAuthResult(nWParam, (bool)nLParam);
	}
	if (nMessageID == WM_WEB_LOAD_RESOURCE) {
		m_ui.RightView->onWebLoadResource();
		m_ui.LeftView->onWebLoadResource();
	}
}

StudentWindow::~StudentWindow()
{
	// 窗口关闭前，保存所有的配置信息...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// 对窗口标题进行修改 => 使用字典模式...
	const char * lpLiveRoomID = config_get_string(App()->GlobalConfig(), "General", "LiveRoomID");
	QString strTitle = QString("%1%2").arg(QTStr("Main.Window.TitleContent")).arg(lpLiveRoomID);
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
	// 弹出退出询问框...
	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmExit.Title"),
		QTStr("ConfirmExit.Quit"));
	if (button == QMessageBox::No) {
		event->ignore();
		return;
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

void StudentWindow::on_actionCameraAdd_triggered()
{
	// 打开摄像头窗口配置 => 添加...
	CDlgPush dlg(this, -1, false);
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
	// 弹出通道配置修改框...
	CDlgPush dlg(this, nDBCameraID, lpViewCamera->IsCameraLogin());
	if (dlg.exec() == QDialog::Rejected)
		return;
	// 点击确定之后的处理过程...
	CWebThread * lpWebThread = App()->GetWebThread();
	GM_MapData & theMapData = dlg.GetPushData();
	// 先在服务器上更新通道，然后将结果更新到界面...
	((lpWebThread != NULL) ? lpWebThread->doWebRegCamera(theMapData) : NULL);
	((lpViewCamera != NULL) ? lpViewCamera->update() : NULL);
	// 直接启动通道 => 通过左侧的焦点窗口...
	m_ui.LeftView->onCameraStart();
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
}
