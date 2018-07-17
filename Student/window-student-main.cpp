
#include <QMessageBox>
#include <QShowEvent>
#include <QDesktopWidget>
#include <QRect>
#include <QScreen>

#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-student-main.h"

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
	m_ui.mainToolBar->addAction(m_ui.actionSystemFullscreen);
	m_ui.mainToolBar->addSeparator();
	m_ui.mainToolBar->addAction(m_ui.actionCameraAdd);
	m_ui.mainToolBar->addAction(m_ui.actionCameraMod);
	m_ui.mainToolBar->addAction(m_ui.actionCameraDel);
	m_ui.mainToolBar->addSeparator();
	m_ui.mainToolBar->addAction(m_ui.actionCameraStart);
	m_ui.mainToolBar->addAction(m_ui.actionCameraStop);
	m_ui.mainToolBar->addSeparator();
	m_ui.mainToolBar->addAction(m_ui.actionPagePrev);
	m_ui.mainToolBar->addAction(m_ui.actionPageJump);
	m_ui.mainToolBar->addAction(m_ui.actionPageNext);
	m_ui.mainToolBar->addSeparator();
	m_ui.mainToolBar->addAction(m_ui.actionHelpAbout);
	// 设置左右窗口的大小比例 => 20:80...
	m_ui.splitter->setStretchFactor(0, 20);
	m_ui.splitter->setStretchFactor(1, 80);
	// 更新主窗口标题栏...
	this->UpdateTitleBar();
	// 恢复窗口坐标位置...
	const char *geometry = config_get_string(App()->GlobalConfig(), "BasicWindow", "geometry");
	if (geometry != NULL) {
		QByteArray byteArray = QByteArray::fromBase64(QByteArray(geometry));
		restoreGeometry(byteArray);

		QRect windowGeometry = normalGeometry();
		if (!WindowPositionValid(windowGeometry)) {
			QRect rect = App()->desktop()->geometry();
			setGeometry(QStyle::alignedRect(
				Qt::LeftToRight,
				Qt::AlignCenter,
				size(), rect));
		}
	}
}

StudentWindow::~StudentWindow()
{
	// 窗口关闭前，保存所有的配置信息...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// 对窗口标题进行修改 => 简化...
	char szTitle[260] = { 0 };
	const char * lpLiveRoomID = config_get_string(App()->GlobalConfig(), "General", "LiveRoomID");
	sprintf(szTitle, "云教室 - 学生端 - 已登录云教室号码：%s", lpLiveRoomID);
	this->setWindowTitle(QString::fromLocal8Bit(szTitle));
}

void StudentWindow::InitWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);

	this->show();
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
}

void StudentWindow::on_actionCameraAdd_triggered()
{
}

void StudentWindow::on_actionCameraMod_triggered()
{
}

void StudentWindow::on_actionCameraDel_triggered()
{
}

void StudentWindow::on_actionCameraStart_triggered()
{
}

void StudentWindow::on_actionCameraStop_triggered()
{
}

void StudentWindow::on_actionPagePrev_triggered()
{
}

void StudentWindow::on_actionPageJump_triggered()
{
}

void StudentWindow::on_actionPageNext_triggered()
{
}

void StudentWindow::on_actionHelpAbout_triggered()
{
}
