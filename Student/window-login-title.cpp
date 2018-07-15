
#include "window-login-title.h"
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QDebug>
#include <QFile>

#define BUTTON_HEIGHT 30		// 按钮高度;
#define BUTTON_WIDTH 30			// 按钮宽度;
#define TITLE_HEIGHT 30			// 标题栏高度;

MyTitleBar::MyTitleBar(QWidget *parent)
	: QWidget(parent)
	, m_isPressed(false)
	, m_isTransparent(false)
	, m_isMoveParentWindow(true)
	, m_bkColor(QColor(153, 153, 153))
{
	// 初始化;
	initControl();
	initConnections();
	// 加载本地样式文件...
	loadStyleSheet(":/login/css/MyTitle.css");
}

MyTitleBar::~MyTitleBar()
{
}

// 初始化控件;
void MyTitleBar::initControl()
{
	m_pIcon = new QLabel;
	m_pTitleContent = new QLabel;

	m_pButtonMin = new QPushButton;
	m_pButtonClose = new QPushButton;

	m_pButtonMin->setFixedSize(QSize(BUTTON_WIDTH, BUTTON_HEIGHT));
	m_pButtonClose->setFixedSize(QSize(BUTTON_WIDTH, BUTTON_HEIGHT));

	m_pTitleContent->setObjectName("TitleContent");
	m_pButtonMin->setObjectName("ButtonMin");
	m_pButtonClose->setObjectName("ButtonClose");

	QHBoxLayout * mylayout = new QHBoxLayout(this);
	mylayout->addWidget(m_pIcon);
	mylayout->addWidget(m_pTitleContent);

	mylayout->addWidget(m_pButtonMin);
	mylayout->addWidget(m_pButtonClose);

	mylayout->setContentsMargins(5, 0, 0, 0);
	mylayout->setSpacing(0);

	m_pTitleContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	this->setFixedHeight(TITLE_HEIGHT);
	this->setWindowFlags(Qt::FramelessWindowHint);
}

// 信号槽的绑定;
void MyTitleBar::initConnections()
{
	connect(m_pButtonMin, SIGNAL(clicked()), this, SLOT(onButtonMinClicked()));
	connect(m_pButtonClose, SIGNAL(clicked()), this, SLOT(onButtonCloseClicked()));
}

// 设置标题栏背景色,在paintEvent事件中进行绘制标题栏背景色;
//在构造函数中给了默认值，可以外部设置颜色值改变标题栏背景色;
void MyTitleBar::setBackgroundColor(QColor inColor, bool isTransparent)
{
	// 保存传递过来的参数...
	m_bkColor = inColor;
	m_isTransparent = isTransparent;
	// 重新绘制（调用paintEvent事件）;
	this->update();
}

// 设置标题栏图标;
void MyTitleBar::setTitleIcon(QString filePath)
{
	QPixmap titleIcon(filePath);
	m_pIcon->setPixmap(titleIcon.scaled(25 , 25));
}

// 设置标题内容;
void MyTitleBar::setTitleContent(QString titleContent, int titleFontSize)
{
	// 设置标题字体大小;
	QFont font = m_pTitleContent->font();
	font.setPointSize(titleFontSize);
	m_pTitleContent->setFont(font);
	// 设置标题内容;
	m_pTitleContent->setText(titleContent);
	m_titleContent = titleContent;
}

// 设置标题栏长度;
void MyTitleBar::setTitleWidth(int width)
{
	this->setFixedWidth(width);
}

// 设置是否通过标题栏移动窗口;
void MyTitleBar::setMoveParentWindowFlag(bool isMoveParentWindow)
{
	m_isMoveParentWindow = isMoveParentWindow;
}

// 设置标题栏上按钮类型;
// 由于不同窗口标题栏上的按钮都不一样，所以可以自定义标题栏中的按钮;
// 这里提供了四个按钮，分别为最小化、还原、最大化、关闭按钮，如果需要其他按钮可自行添加设置;
/*void MyTitleBar::setButtonType(ButtonType buttonType)
{
	m_buttonType = buttonType;

	switch (buttonType)
	{
	case MIN_BUTTON:
		{
			m_pButtonRestore->setVisible(false);
			m_pButtonMax->setVisible(false);
		}
		break;
	case MIN_MAX_BUTTON:
		{
			m_pButtonRestore->setVisible(false);
		}
		break;
	case ONLY_CLOSE_BUTTON:
		{
			m_pButtonMin->setVisible(false);
			m_pButtonRestore->setVisible(false);
			m_pButtonMax->setVisible(false);
		}
		break;
	default:
		break;
	}
}

// 设置标题栏中的标题是否会自动滚动，跑马灯的效果;
// 一般情况下标题栏中的标题内容是不滚动的，但是既然自定义就看自己需要嘛，想怎么设计就怎么搞O(∩_∩)O！
void MyTitleBar::setTitleRoll()
{
	connect(&m_titleRollTimer, SIGNAL(timeout()), this, SLOT(onRollTitle()));
	m_titleRollTimer.start(200);
}*/

// 绘制标题栏背景色;
void MyTitleBar::paintEvent(QPaintEvent *event)
{
	if (!m_isTransparent)
	{
		// 设置背景色;
		QPainter painter(this);
		QPainterPath pathBack;
		pathBack.setFillRule(Qt::WindingFill);
		pathBack.addRoundedRect(QRect(0, 0, this->width(), this->height()), 3, 3);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.fillPath(pathBack, QBrush(m_bkColor));
	}	

	QWidget::paintEvent(event);
}

// 双击响应事件，主要是实现双击标题栏进行最大化和最小化操作;
void MyTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
	// 只有存在最大化、还原按钮时双击才有效;
	/*if (m_buttonType == MIN_MAX_BUTTON)
	{
		// 通过最大化按钮的状态判断当前窗口是处于最大化还是原始大小状态;
		// 或者通过单独设置变量来表示当前窗口状态;
		if (m_pButtonMax->isVisible())
		{
			onButtonMaxClicked();
		}
		else
		{
			onButtonRestoreClicked();
		}
	}*/

	return QWidget::mouseDoubleClickEvent(event);
}
//
// 以下通过 mousePressEvent | mouseMoveEvent | mouseReleaseEvent 三个事件实现了鼠标拖动标题栏移动窗口的效果;
void MyTitleBar::mousePressEvent(QMouseEvent *event)
{
	m_isPressed = true;
	m_startMovePos = event->globalPos();
	qDebug() << "MyTitleBar::mousePressEvent" << m_startMovePos.x() << m_startMovePos.y();
	
	return QWidget::mousePressEvent(event);
}

void MyTitleBar::mouseMoveEvent(QMouseEvent *event)
{
	if (m_isPressed && m_isMoveParentWindow && this->parentWidget()) {
		QPoint movePoint = event->globalPos() - m_startMovePos;
		QPoint widgetPos = this->parentWidget()->pos() + movePoint;
		m_startMovePos = event->globalPos();
		qDebug() << "MyTitleBar::mouseMoveEvent" << widgetPos.x() << widgetPos.y();
		this->parentWidget()->move(widgetPos.x(), widgetPos.y());
	}
	return QWidget::mouseMoveEvent(event);
}

void MyTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
	m_isPressed = false;
	return QWidget::mouseReleaseEvent(event);
}

// 加载本地样式文件;
// 可以将样式直接写在文件中，程序运行时直接加载进来;
void MyTitleBar::loadStyleSheet(const QString &sheetName)
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

// 以下为按钮操作响应的槽;
void MyTitleBar::onButtonMinClicked()
{
	emit signalButtonMinClicked();
}

void MyTitleBar::onButtonCloseClicked()
{
	emit signalButtonCloseClicked();
}

// 该方法主要是让标题栏中的标题显示为滚动的效果;
/*void MyTitleBar::onRollTitle()
{
	static int nPos = 0;
	QString titleContent = m_titleContent;
	// 当截取的位置比字符串长时，从头开始;
	if (nPos > titleContent.length())
		nPos = 0;

	m_pTitleContent->setText(titleContent.mid(nPos));
	nPos++;
}*/