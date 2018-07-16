
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
	// ���ز˵��͹�����...
	m_ui.menuBar->setVisible(false);
	m_ui.mainToolBar->setVisible(false);
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