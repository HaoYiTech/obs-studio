
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

// 设置云台窗口的标题栏文字信息...
void CPTZWindow::doUpdatePTZ(int nDBCameraID)
{
	// 先直接更新云台标题栏...
	m_nDBCameraID = nDBCameraID;
	ui->titleName->setText(QString("%1 - %2%3")
		.arg(QTStr("PTZ.Window.TitleContent"))
		.arg(QStringLiteral("ID: "))
		.arg(nDBCameraID));
	// 再根据编号查找通道，并更新图像翻转状态...
	StudentWindow * lpWndStudent = dynamic_cast<StudentWindow *>(this->parent());
	CViewLeft * lpViewLeft = ((lpWndStudent != NULL) ? lpWndStudent->GetViewLeft() : NULL);
	CViewCamera * lpViewCamera = ((lpViewLeft != NULL) ? lpViewLeft->FindDBCameraByID(m_nDBCameraID) : NULL);
	(lpViewCamera != NULL) ? this->doUpdateImageFlip(lpViewCamera->GetImageFilpVal()) : NULL;
}

void CPTZWindow::doUpdateImageFlip(string & inFlipVal)
{
	// 根据返回的IPC配置情况，获取下拉框的索引编号...
	int nIndex = ((stricmp(inFlipVal.c_str(), "true") == 0) ? 1 : 0);
	// 避免激发currentIndexChanged引起混乱，需要关闭信号，然后恢复...
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
	// FramelessWindowHint属性设置窗口去除边框;
	// WindowMinimizeButtonHint 属性设置在窗口最小化时，点击任务栏窗口可以显示出原窗口;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// 设置窗口背景透明;
	setAttribute(Qt::WA_TranslucentBackground);
	// 关闭窗口时释放资源;
	setAttribute(Qt::WA_DeleteOnClose);
	// 重新设定窗口大小...
	this->resize(240, 450);
	// 加载PTZ窗口的层叠显示样式表...
	this->loadStyleSheet(":/ptz/css/PTZWindow.css");
	// 关联点击关闭按钮|滑块变化的信号槽事件...
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->speedSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
	// 关联云台方向按钮的按下|抬起信号槽事件...
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
	// 直接更新转数的数值内容...
	int nValue = ui->speedSlider->value();
	this->onSliderChanged(nValue);
	// 设置镜像下拉框内容...
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

// 绘制圆角窗口背景...
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

// 以下通过 mousePressEvent | mouseMoveEvent | mouseReleaseEvent 三个事件实现了鼠标拖动标题栏移动窗口的效果;
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
