#pragma once

#include <QDialog>
#include "HYDefine.h"
#include "ui_DlgSetSys.h"

class CDlgSetSys : public QDialog {
	Q_OBJECT
public:
	CDlgSetSys(QWidget *parent);
	virtual ~CDlgSetSys();
private slots:
	void	accept();
	void	reject();
private:
	Ui::DlgSetSys		m_ui;           // QT的界面对象
};
