#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_StudentWindow.h"

class StudentWindow : public QMainWindow
{
	Q_OBJECT
public:
	StudentWindow(QWidget *parent = Q_NULLPTR);
	~StudentWindow();
public:
	void	InitWindow();
	void	UpdateTitleBar();
protected:
	virtual void closeEvent(QCloseEvent *event) override;
private:
	Ui::StudentClass m_ui;
};
