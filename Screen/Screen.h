#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Screen.h"

class Screen : public QMainWindow
{
	Q_OBJECT

public:
	Screen(QWidget *parent = Q_NULLPTR);

private:
	Ui::ScreenClass ui;
};
