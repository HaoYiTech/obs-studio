#pragma once

#include <QDialog>
#include "HYDefine.h"
#include "ui_DlgAbout.h"

class CDlgAbout : public QDialog {
	Q_OBJECT
public:
	CDlgAbout(QWidget *parent);
	virtual ~CDlgAbout();
private slots:
	void	accept();
private:
	Ui::DlgAbout		m_ui;           // QT的界面对象
};
