
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
	// 隐藏菜单和工具条...
	m_ui.menuBar->setVisible(false);
	m_ui.mainToolBar->setVisible(false);
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