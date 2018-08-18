#pragma once

#include <QDialog>
#include "HYDefine.h"
#include "ui_DlgPush.h"

class CDlgPush : public QDialog {
	Q_OBJECT
public:
	CDlgPush(QWidget *parent, int nDBCameraID, bool bIsOffLine);
	virtual ~CDlgPush();
public:
	GM_MapData & GetPushData() { return m_MapData; }
private slots:
	void	accept();
	void	reject();
private:
	GM_MapData		m_MapData;		// 存放通道配置信息
	int             m_nDBCameraID;
	bool			m_bEdit;
	Ui::DlgPush		m_ui;
};
