
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
	// FramelessWindowHint�������ô���ȥ���߿�;
	// WindowMinimizeButtonHint ���������ڴ�����С��ʱ��������������ڿ�����ʾ��ԭ����;
	//Qt::WindowFlags flag = this->windowFlags();
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	// ���ô��ڱ���͸��;
	setAttribute(Qt::WA_TranslucentBackground);
	// �رմ���ʱ�ͷ���Դ;
	setAttribute(Qt::WA_DeleteOnClose);
	// �����趨���ڴ�С...
	this->resize(240, 400);
	// ����PTZ���ڵĲ����ʾ��ʽ��...
	this->loadStyleSheet(":/ptz/css/PTZWindow.css");
	// ��������رհ�ť���źŲ��¼�...
	connect(ui->btnClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
	connect(ui->speedSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
	// ֱ�Ӹ���ת������ֵ����...
	int nValue = ui->speedSlider->value();
	this->onSliderChanged(nValue);
}

void CPTZWindow::initMyTitle()
{
	// ������̨���ڵı�����������Ϣ...
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
