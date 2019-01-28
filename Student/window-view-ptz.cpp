
#include "window-view-camera.hpp"
#include "window-view-ptz.h"
#include "student-app.h"
#include <QMouseEvent>
#include <QPainter>

CPTZWindow::CPTZWindow(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::PTZWindow)
{
	ui->setupUi(this);
	this->initWindow();
	this->doUpdatePTZ(m_nDBCameraID);
}

CPTZWindow::~CPTZWindow()
{
}

// ������̨���ڵı�����������Ϣ...
void CPTZWindow::doUpdatePTZ(int nDBCameraID)
{
	// ��ֱ�Ӹ�����̨������...
	m_nDBCameraID = nDBCameraID;
	ui->titleName->setText(QString("%1 - %2%3")
		.arg(QTStr("PTZ.Window.TitleContent"))
		.arg(QStringLiteral("ID: "))
		.arg(nDBCameraID));
	// �ٸ��ݱ�Ų���ͨ����������ͼ��ת״̬...
	StudentWindow * lpWndStudent = dynamic_cast<StudentWindow *>(this->parent());
	CViewLeft * lpViewLeft = ((lpWndStudent != NULL) ? lpWndStudent->GetViewLeft() : NULL);
	CViewCamera * lpViewCamera = ((lpViewLeft != NULL) ? lpViewLeft->FindDBCameraByID(m_nDBCameraID) : NULL);
	(lpViewCamera != NULL) ? this->doUpdateImageFlip(lpViewCamera->GetImageFilpVal()) : NULL;
}

void CPTZWindow::doUpdateImageFlip(string & inFlipVal)
{
	// ���ݷ��ص�IPC�����������ȡ��������������...
	int nIndex = ((stricmp(inFlipVal.c_str(), "true") == 0) ? 1 : 0);
	// ���⼤��currentIndexChanged������ң���Ҫ�ر��źţ�Ȼ��ָ�...
	ui->comboFlip->blockSignals(true);
	ui->comboFlip->setCurrentIndex(nIndex);
	ui->comboFlip->blockSignals(false);
}

void CPTZWindow::loadStyleSheet(const QString &sheetName)
{
	QFile file(sheetName);
	file.open(QFile::ReadOnly);
	if (file.isOpen())
	{
		QString styleSheet = this->styleSheet();
		styleSheet += QLatin1String(file.readAll());
		this->setStyleSheet(styleSheet);
	}
}

void CPTZWindow::initWindow()
{
	// FramelessWindowHint�������ô���ȥ���߿�;
	// WindowMinimizeButtonHint ���������ڴ�����С��ʱ��������������ڿ�����ʾ��ԭ����;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// ���ô��ڱ���͸��;
	setAttribute(Qt::WA_TranslucentBackground);
	// �رմ���ʱ�ͷ���Դ;
	setAttribute(Qt::WA_DeleteOnClose);
	// �����趨���ڴ�С...
	this->resize(240, 450);
	// ����PTZ���ڵĲ����ʾ��ʽ��...
	this->loadStyleSheet(":/ptz/css/PTZWindow.css");
	// ��������رհ�ť|����仯���źŲ��¼�...
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->speedSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
	// ������̨����ť�İ���|̧���źŲ��¼�...
	connect(ui->btnTop, SIGNAL(pressed()), this, SLOT(onBtnUpPressed()));
	connect(ui->btnTop, SIGNAL(released()), this, SLOT(onTiltFinish()));
	connect(ui->btnBot, SIGNAL(pressed()), this, SLOT(onBtnDownPressed()));
	connect(ui->btnBot, SIGNAL(released()), this, SLOT(onTiltFinish()));
	connect(ui->btnLeft, SIGNAL(pressed()), this, SLOT(onBtnLeftPressed()));
	connect(ui->btnLeft, SIGNAL(released()), this, SLOT(onPanFinish()));
	connect(ui->btnRight, SIGNAL(pressed()), this, SLOT(onBtnRightPressed()));
	connect(ui->btnRight, SIGNAL(released()), this, SLOT(onPanFinish()));
	connect(ui->addZoom, SIGNAL(pressed()), this, SLOT(onAddZoomPressed()));
	connect(ui->addZoom, SIGNAL(released()), this, SLOT(onZoomFinish()));
	connect(ui->subZoom, SIGNAL(pressed()), this, SLOT(onSubZoomPressed()));
	connect(ui->subZoom, SIGNAL(released()), this, SLOT(onZoomFinish()));
	connect(ui->addFocus, SIGNAL(pressed()), this, SLOT(onAddFocusPressed()));
	connect(ui->addFocus, SIGNAL(released()), this, SLOT(onFocusFinish()));
	connect(ui->subFocus, SIGNAL(pressed()), this, SLOT(onSubFocusPressed()));
	connect(ui->subFocus, SIGNAL(released()), this, SLOT(onFocusFinish()));
	connect(ui->addIris, SIGNAL(pressed()), this, SLOT(onAddIrisPressed()));
	connect(ui->addIris, SIGNAL(released()), this, SLOT(onIrisFinish()));
	connect(ui->subIris, SIGNAL(pressed()), this, SLOT(onSubIrisPressed()));
	connect(ui->subIris, SIGNAL(released()), this, SLOT(onIrisFinish()));
	// ֱ�Ӹ���ת������ֵ����...
	int nValue = ui->speedSlider->value();
	this->onSliderChanged(nValue);
	// ���þ�������������...
	ui->comboFlip->addItem(QTStr("Image.Flip.Close"), false);
	ui->comboFlip->addItem(QTStr("Image.Flip.Center"), true);
	connect(ui->comboFlip, SIGNAL(currentIndexChanged(int)), this, SLOT(onFlipIndexChanged(int)));
}

void CPTZWindow::onFlipIndexChanged(int index)
{
	bool bResult = this->doPTZCmd(kIMAGE_FLIP, index);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("Image.Flip.Opt"))
		.arg(QTStr((index <= 0) ? "Image.Flip.Close" : "Image.Flip.Open"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}

bool CPTZWindow::doPTZCmd(CMD_ISAPI inCMD, int inSpeedVal)
{
	StudentWindow * lpWndStudent = dynamic_cast<StudentWindow *>(this->parent());
	CViewLeft * lpViewLeft = ((lpWndStudent != NULL) ? lpWndStudent->GetViewLeft() : NULL);
	CViewCamera * lpViewCamera = ((lpViewLeft != NULL) ? lpViewLeft->FindDBCameraByID(m_nDBCameraID) : NULL);
	return ((lpViewCamera != NULL) ? lpViewCamera->doPTZCmd(inCMD, inSpeedVal) : false);
}

void CPTZWindow::onBtnUpPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_Y_TILT, m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Operate"))
		.arg(QTStr("PTZ.Tips.Up"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onBtnDownPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_Y_TILT, -m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Operate"))
		.arg(QTStr("PTZ.Tips.Down"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onTiltFinish()
{
	this->doPTZCmd(kPTZ_Y_TILT, 0);
}

void CPTZWindow::onBtnRightPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_X_PAN, m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Operate"))
		.arg(QTStr("PTZ.Tips.Right"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onBtnLeftPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_X_PAN, -m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Operate"))
		.arg(QTStr("PTZ.Tips.Left"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onPanFinish()
{
	this->doPTZCmd(kPTZ_X_PAN, 0);
}

void CPTZWindow::onAddZoomPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_Z_ZOOM, m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Zoom"))
		.arg(QTStr("PTZ.Tips.Add"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onSubZoomPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_Z_ZOOM, -m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Zoom"))
		.arg(QTStr("PTZ.Tips.Sub"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onZoomFinish()
{
	this->doPTZCmd(kPTZ_Z_ZOOM, 0);
}

void CPTZWindow::onAddFocusPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_F_FOCUS, m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Focus"))
		.arg(QTStr("PTZ.Tips.Add"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onSubFocusPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_F_FOCUS, -m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Focus"))
		.arg(QTStr("PTZ.Tips.Sub"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onFocusFinish()
{
	this->doPTZCmd(kPTZ_F_FOCUS, 0);
}

void CPTZWindow::onAddIrisPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_I_IRIS, m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Iris"))
		.arg(QTStr("PTZ.Tips.Add"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onSubIrisPressed()
{
	bool bResult = this->doPTZCmd(kPTZ_I_IRIS, -m_nSpeedValue);
	ui->errTitle->setText(QString("%1%2%3")
		.arg(QTStr("PTZ.Title.Iris"))
		.arg(QTStr("PTZ.Tips.Sub"))
		.arg(QTStr(bResult ? "PTZ.Success" : "PTZ.Error")));
	ui->errTitle->setStyleSheet(bResult ? "color:yellow" : "color:red");
}
void CPTZWindow::onIrisFinish()
{
	this->doPTZCmd(kPTZ_I_IRIS, 0);
}

void CPTZWindow::onButtonCloseClicked()
{
	this->hide();
}

void CPTZWindow::onSliderChanged(int value)
{
	ui->speedNum->setText(QString("%1").arg(value));
	m_nSpeedValue = value;
}

// ����Բ�Ǵ��ڱ���...
void CPTZWindow::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QPainterPath pathBack, pathPan;
	pathBack.setFillRule(Qt::WindingFill);
	painter.setRenderHint(QPainter::Antialiasing, true);
	pathBack.addRoundedRect(QRect(0, 0, this->width(), this->height()), 4, 4);
	painter.fillPath(pathBack, QBrush(QPixmap(":/ptz/images/ptz/back.png").scaled(this->size())));
	QPixmap thePixPan = QPixmap(":/ptz/images/ptz/pan.png");
	int nXPos = (this->size().width() - thePixPan.size().width()) / 2;
	int nYPos = ui->hori_title->totalSizeHint().height();
	painter.drawPixmap(nXPos, nYPos, thePixPan);
	QWidget::paintEvent(event);
}

// ����ͨ�� mousePressEvent | mouseMoveEvent | mouseReleaseEvent �����¼�ʵ��������϶��������ƶ����ڵ�Ч��;
void CPTZWindow::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		m_isPressed = true;
		m_startMovePos = event->globalPos();
	}
	return QWidget::mousePressEvent(event);
}

void CPTZWindow::mouseMoveEvent(QMouseEvent *event)
{
	if (m_isPressed) {
		QPoint movePoint = event->globalPos() - m_startMovePos;
		QPoint widgetPos = this->pos() + movePoint;
		m_startMovePos = event->globalPos();
		this->move(widgetPos.x(), widgetPos.y());
	}
	return QWidget::mouseMoveEvent(event);
}

void CPTZWindow::mouseReleaseEvent(QMouseEvent *event)
{
	m_isPressed = false;
	return QWidget::mouseReleaseEvent(event);
}
