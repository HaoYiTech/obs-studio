
#include <QMessageBox>
#include <QShowEvent>
#include <QDesktopWidget>
#include <QRect>
#include <QScreen>

#include "HYDefine.h"
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
	// ����ͨ���˵���ť...
	m_ui.mainToolBar->addAction(m_ui.actionCameraAdd);
	m_ui.mainToolBar->addAction(m_ui.actionCameraMod);
	m_ui.mainToolBar->addAction(m_ui.actionCameraDel);
	m_ui.mainToolBar->addSeparator();
	// ����ͨ���˵���ťΪ������...
	m_ui.actionCameraAdd->setDisabled(true);
	m_ui.actionCameraMod->setDisabled(true);
	m_ui.actionCameraDel->setDisabled(true);
	// ����ͨ���˵���ť�źŲ�...
	this->connect(m_ui.LeftView, SIGNAL(enableCameraAdd(bool)), m_ui.actionCameraAdd, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraMod(bool)), m_ui.actionCameraMod, SLOT(setEnabled(bool)));
	this->connect(m_ui.LeftView, SIGNAL(enableCameraDel(bool)), m_ui.actionCameraDel, SLOT(setEnabled(bool)));
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
	// ���ڹر�ǰ���������е�������Ϣ...
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void StudentWindow::UpdateTitleBar()
{
	// �Դ��ڱ�������޸� => ʹ���ֵ�ģʽ...
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
