#pragma once

#include <QWidget>
#include <obs.hpp>

class OBSQTDisplay : public QWidget {
	Q_OBJECT

	OBSDisplay display;

	void CreateDisplay();

	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

signals:
	void DisplayCreated(OBSQTDisplay *window);
	void DisplayResized();

public:
	OBSQTDisplay(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	inline obs_display_t *GetDisplay() const {return display;}
	virtual QPaintEngine *paintEngine() const override;
};
