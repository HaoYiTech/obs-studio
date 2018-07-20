#pragma once

#include <QWidget>

class OBSQTDisplay : public QWidget {
	Q_OBJECT
public:
	OBSQTDisplay(QWidget *parent = 0, Qt::WindowFlags flags = 0);
public:
	void   doCaptureFocus();
	void   doReleaseFocus();
	void   setBKColor(QColor inColor);
protected:
	void   paintEvent(QPaintEvent *event) override;
protected:
	QColor		m_bkColor;
	bool		m_bFocus;
};
