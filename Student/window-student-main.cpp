
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
	// ���ý������...
	m_ui.setupUi(this);
	// ������������ʾ״̬...
	m_ui.actionSystemToolbar->setCheckable(true);
	m_ui.actionSystemToolbar->setChecked(true);
	// �����������˵��¼���Ӧ...
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
	// �������Ҵ��ڵĴ�С���� => 20:80...
	m_ui.splitter->setStretchFactor(0, 20);
	m_ui.splitter->setStretchFactor(1, 80);
	// ���������ڱ�����...
	this->UpdateTitleBar();
	// �ָ���������λ��...
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
	// ���ڹر�ǰ���������е�������Ϣ...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// �Դ��ڱ�������޸� => ��...
	char szTitle[260] = { 0 };
	const char * lpLiveRoomID = config_get_string(App()->GlobalConfig(), "General", "LiveRoomID");
	sprintf(szTitle, "�ƽ��� - ѧ���� - �ѵ�¼�ƽ��Һ��룺%s", lpLiveRoomID);
	this->setWindowTitle(QString::fromLocal8Bit(szTitle));
}

void StudentWindow::InitWindow()
{
	blog(LOG_INFO, STARTUP_SEPARATOR);

	this->show();
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
