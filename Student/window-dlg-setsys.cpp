
#include <QCloseEvent>
#include "web-thread.h"
#include "SocketUtils.h"
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-setsys.hpp"

CDlgSetSys::CDlgSetSys(QWidget *parent)
  : QDialog (parent)
{
	// 设置界面对象...
	m_ui.setupUi(this);
	// 设置标题栏文字内容，设置窗口图标...
	this->setWindowTitle(QTStr("SetSys.Title"));
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// 设置相关内容信息 => 需要从UTF8转换成Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	string & strMainName = App()->GetMainName();
	os_utf8_to_wcs(strMainName.c_str(), strMainName.size(), szWBuffer, MAX_PATH);
	m_ui.nameSet->setText(QString((QChar*)szWBuffer));
	// 设置相关内容信息 => 需要从UTF8转换成Unicode...
	string & strMacMD5 = App()->GetAuthMacMD5();
	os_utf8_to_wcs(strMacMD5.c_str(), strMacMD5.size(), szWBuffer, MAX_PATH);
	m_ui.stickName->setStyleSheet("background-color:#f0f0f0;color:darkblue;");
	m_ui.stickName->setText(QString((QChar*)szWBuffer));
	// 本机身份角色的信息显示 => 根据服务器配置设定初始值...
	m_ui.comRole->addItem(QTStr("Student.RoleWanRecv"), QVariant(kRoleWanRecv));
	m_ui.comRole->addItem(QTStr("Student.RoleMultiRecv"), QVariant(kRoleMultiRecv));
	m_ui.comRole->addItem(QTStr("Student.RoleMultiSend"), QVariant(kRoleMultiSend));
	m_ui.comRole->setCurrentIndex((int)(App()->GetRoleType()));
	// 获取从服务器得到的组播发送接口配置...
	int nCurSelIndex = 0, nCurPosIndex = 0;
	string & strIPSend = App()->GetMultiIPSendAddr();
	// 先加入一个默认的组播发送接口地址 => INADDR_ANY => 0
	m_ui.comIPSend->addItem(QTStr("SetSys.Default"), QVariant("0"));
	if (::strcmp(strIPSend.c_str(), "0") == 0) {
		nCurSelIndex = 0;
	}
	// 遍历本机IP地址，显示当前选中IP地址...
	UINT nNumIPAddr = SocketUtils::GetNumIPAddrs();
	StrPtrLen * lpPtrIPAddr = NULL;
	for (UINT i = 0; i < nNumIPAddr; ++i) {
		lpPtrIPAddr = SocketUtils::GetIPAddrStr(i);
		// 如果本机IP地址是127.0.0.1，直接跳过...
		if (::strcmp(lpPtrIPAddr->Ptr, "127.0.0.1") == 0)
			continue;
		// 累加索引编号...
		++nCurPosIndex;
		// 添加IP地址到列表框当中...
		m_ui.comIPSend->addItem(lpPtrIPAddr->Ptr, QVariant(lpPtrIPAddr->Ptr));
		// 如果服务器设定的IP地址与当前IP地址一致，设置选中索引编号...
		if (::strcmp(strIPSend.c_str(), lpPtrIPAddr->Ptr) == 0) {
			nCurSelIndex = nCurPosIndex;
		}
	}
	// 最终设定当前选中的索引编号...
	m_ui.comIPSend->setCurrentIndex(nCurSelIndex);
}

CDlgSetSys::~CDlgSetSys()
{
}

// 点击确认按钮...
void CDlgSetSys::accept()
{
	// 验证输入数据的有效性...
	int nCurIndex = m_ui.comIPSend->currentIndex();
	QString & strQNameSet = m_ui.nameSet->text();
	if (strQNameSet.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.Name.Empty"));
		m_ui.nameSet->setFocus();
		return;
	}
	// 当前索引编号和保存数据必须有效...
	QString strQIPSend = m_ui.comIPSend->itemData(nCurIndex).toString();
	if (nCurIndex < 0 || strQIPSend.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.IPSend.Empty"));
		return;
	}
	// 进一步转换获取到的数据...
	ROLE_TYPE nRoleType = (ROLE_TYPE)m_ui.comRole->currentIndex();
	string strMainName = string((const char *)strQNameSet.toUtf8());
	string strIPSend = string((const char*)strQIPSend.toUtf8());
	CWebThread * lpWebThread = App()->GetWebThread();
	// 向网站服务器保存配置失败，弹框提示，然后返回...
	if (!lpWebThread->doWebSaveSys(nRoleType, strMainName, strIPSend)) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.Post.Error"));
		return;
	}
	// 保存成功，更新全局配置...
	App()->SetRoleType(nRoleType);
	App()->SetMainName(strMainName);
	App()->SetMultiIPSendAddr(strIPSend);
	// 调用接口，退出对话框...
	this->done(DialogCode::Accepted);
}

// 点击取消按钮...
void CDlgSetSys::reject()
{
	this->done(DialogCode::Rejected);
}