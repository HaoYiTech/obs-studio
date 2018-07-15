#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Student.h"

class Student : public QMainWindow
{
	Q_OBJECT

public:
	Student(QWidget *parent = Q_NULLPTR);

private:
	Ui::StudentClass ui;
};
