
#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPushButton>

/*enum ButtonType
{
	MIN_BUTTON = 0,			// 最小化和关闭按钮;
	MIN_MAX_BUTTON ,		// 最小化、最大化和关闭按钮;
	ONLY_CLOSE_BUTTON		// 只有关闭按钮;
};*/

class MyTitleBar : public QWidget
{
	Q_OBJECT
public:
	MyTitleBar(QWidget *parent);
	~MyTitleBar();

	// 设置标题栏背景色;
	void setBackgroundColor(QColor inColor, bool isTransparent = false);
	// 设置标题栏图标;
	void setTitleIcon(QString filePath);
	// 设置标题内容;
	void setTitleContent(QString titleContent , int titleFontSize = 9);
	// 设置标题栏长度;
	void setTitleWidth(int width);

	// 设置是否通过标题栏移动窗口;
	void setMoveParentWindowFlag(bool isMoveParentWindow);
private:
	void paintEvent(QPaintEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	// 初始化控件;
	void initControl();
	// 信号槽的绑定;
	void initConnections();
	// 加载样式文件;
	void loadStyleSheet(const QString &sheetName);

signals:
	// 按钮触发的信号;
	void signalButtonMinClicked();
	void signalButtonCloseClicked();
private slots:
	// 按钮触发的槽;
	void onButtonMinClicked();
	void onButtonCloseClicked();

private:
	QLabel* m_pIcon;					// 标题栏图标;
	QLabel* m_pTitleContent;			// 标题栏内容;
	QPushButton* m_pButtonMin;			// 最小化按钮;
	QPushButton* m_pButtonClose;		// 关闭按钮;
	
	// 标题栏背景色;
	QColor m_bkColor;

	// 移动窗口的变量;
	bool m_isPressed;
	QPoint m_startMovePos;
	// 标题栏内容;
	QString m_titleContent;
	// 标题栏是否透明;
	bool m_isTransparent;
	// 是否通过标题栏移动窗口;
	bool m_isMoveParentWindow;
};
