
#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPushButton>

/*enum ButtonType
{
	MIN_BUTTON = 0,			// ��С���͹رհ�ť;
	MIN_MAX_BUTTON ,		// ��С������󻯺͹رհ�ť;
	ONLY_CLOSE_BUTTON		// ֻ�йرհ�ť;
};*/

class MyTitleBar : public QWidget
{
	Q_OBJECT
public:
	MyTitleBar(QWidget *parent);
	~MyTitleBar();

	// ���ñ���������ɫ;
	void setBackgroundColor(QColor inColor, bool isTransparent = false);
	// ���ñ�����ͼ��;
	void setTitleIcon(QString filePath);
	// ���ñ�������;
	void setTitleContent(QString titleContent , int titleFontSize = 9);
	// ���ñ���������;
	void setTitleWidth(int width);

	// �����Ƿ�ͨ���������ƶ�����;
	void setMoveParentWindowFlag(bool isMoveParentWindow);
private:
	void paintEvent(QPaintEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	// ��ʼ���ؼ�;
	void initControl();
	// �źŲ۵İ�;
	void initConnections();
	// ������ʽ�ļ�;
	void loadStyleSheet(const QString &sheetName);

signals:
	// ��ť�������ź�;
	void signalButtonMinClicked();
	void signalButtonCloseClicked();
private slots:
	// ��ť�����Ĳ�;
	void onButtonMinClicked();
	void onButtonCloseClicked();

private:
	QLabel* m_pIcon;					// ������ͼ��;
	QLabel* m_pTitleContent;			// ����������;
	QPushButton* m_pButtonMin;			// ��С����ť;
	QPushButton* m_pButtonClose;		// �رհ�ť;
	
	// ����������ɫ;
	QColor m_bkColor;

	// �ƶ����ڵı���;
	bool m_isPressed;
	QPoint m_startMovePos;
	// ����������;
	QString m_titleContent;
	// �������Ƿ�͸��;
	bool m_isTransparent;
	// �Ƿ�ͨ���������ƶ�����;
	bool m_isMoveParentWindow;
};
