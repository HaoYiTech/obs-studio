
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
	this->initMyTitle();
}

CPTZWindow::~CPTZWindow()
{
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
	this->resize(240, 400);
	// 加载PTZ窗口的层叠显示样式表...
	this->loadStyleSheet(":/ptz/css/PTZWindow.css");
	// 关联点击关闭按钮的信号槽事件...
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->speedSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
	// 直接更新转数的数值内容...
	int nValue = ui->speedSlider->value();
	this->onSliderChanged(nValue);
}

void CPTZWindow::initMyTitle()
{
	// 设置云台窗口的标题栏文字信息...
	ui->titleName->setText(QTStr("PTZ.Window.TitleContent"));
}

void CPTZWindow::onButtonCloseClicked()
{
	this->hide();
}

void CPTZWindow::onSliderChanged(int value)
{
	ui->speedNum->setText(QString("%1").arg(value));
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
