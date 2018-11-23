#pragma once

#include <QDialog>
#include "HYDefine.h"
#include "ui_DlgPush.h"

class CDlgPush : public QDialog {
	Q_OBJECT
public:
	CDlgPush(QWidget *parent, int nDBCameraID);
	virtual ~CDlgPush();
public:
	GM_MapData & GetPushData() { return m_MapData; }
private slots:
	void	accept();
	void	reject();
private:
	GM_MapData		m_MapData;      // 存放通道配置信息
	int             m_nDBCameraID;  // 通道数据库编号
	bool			m_bEdit;        // 是否处于编辑状态
	Ui::DlgPush		m_ui;           // QT的界面对象
};
