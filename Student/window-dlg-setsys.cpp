
#include <QCloseEvent>
#include "web-thread.h"
#include "SocketUtils.h"
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-setsys.hpp"

CDlgSetSys::CDlgSetSys(QWidget *parent)
  : QDialog (parent)
{
	// ���ý������...
	m_ui.setupUi(this);
	// ���ñ������������ݣ����ô���ͼ��...
	this->setWindowTitle(QTStr("SetSys.Title"));
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// �������������Ϣ => ��Ҫ��UTF8ת����Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	string & strMainName = App()->GetMainName();
	os_utf8_to_wcs(strMainName.c_str(), strMainName.size(), szWBuffer, MAX_PATH);
	m_ui.nameSet->setText(QString((QChar*)szWBuffer));
	// �������������Ϣ => ��Ҫ��UTF8ת����Unicode...
	string & strMacMD5 = App()->GetAuthMacMD5();
	os_utf8_to_wcs(strMacMD5.c_str(), strMacMD5.size(), szWBuffer, MAX_PATH);
	m_ui.stickName->setStyleSheet("background-color:#f0f0f0;color:darkblue;");
	m_ui.stickName->setText(QString((QChar*)szWBuffer));
	// ������ݽ�ɫ����Ϣ��ʾ => ���ݷ����������趨��ʼֵ...
	m_ui.comRole->addItem(QTStr("Student.RoleWanRecv"), QVariant(kRoleWanRecv));
	m_ui.comRole->addItem(QTStr("Student.RoleMultiRecv"), QVariant(kRoleMultiRecv));
	m_ui.comRole->addItem(QTStr("Student.RoleMultiSend"), QVariant(kRoleMultiSend));
	m_ui.comRole->setCurrentIndex((int)(App()->GetRoleType()));
	// ��ȡ�ӷ������õ����鲥���ͽӿ�����...
	int nCurSelIndex = 0, nCurPosIndex = 0;
	string & strIPSend = App()->GetMultiIPSendAddr();
	// �ȼ���һ��Ĭ�ϵ��鲥���ͽӿڵ�ַ => INADDR_ANY => 0
	m_ui.comIPSend->addItem(QTStr("SetSys.Default"), QVariant("0"));
	if (::strcmp(strIPSend.c_str(), "0") == 0) {
		nCurSelIndex = 0;
	}
	// ��������IP��ַ����ʾ��ǰѡ��IP��ַ...
	UINT nNumIPAddr = SocketUtils::GetNumIPAddrs();
	StrPtrLen * lpPtrIPAddr = NULL;
	for (UINT i = 0; i < nNumIPAddr; ++i) {
		lpPtrIPAddr = SocketUtils::GetIPAddrStr(i);
		// �������IP��ַ��127.0.0.1��ֱ������...
		if (::strcmp(lpPtrIPAddr->Ptr, "127.0.0.1") == 0)
			continue;
		// �ۼ��������...
		++nCurPosIndex;
		// ���IP��ַ���б����...
		m_ui.comIPSend->addItem(lpPtrIPAddr->Ptr, QVariant(lpPtrIPAddr->Ptr));
		// ����������趨��IP��ַ�뵱ǰIP��ַһ�£�����ѡ���������...
		if (::strcmp(strIPSend.c_str(), lpPtrIPAddr->Ptr) == 0) {
			nCurSelIndex = nCurPosIndex;
		}
	}
	// �����趨��ǰѡ�е��������...
	m_ui.comIPSend->setCurrentIndex(nCurSelIndex);
}

CDlgSetSys::~CDlgSetSys()
{
}

// ���ȷ�ϰ�ť...
void CDlgSetSys::accept()
{
	// ��֤�������ݵ���Ч��...
	int nCurIndex = m_ui.comIPSend->currentIndex();
	QString & strQNameSet = m_ui.nameSet->text();
	if (strQNameSet.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.Name.Empty"));
		m_ui.nameSet->setFocus();
		return;
	}
	// ��ǰ������źͱ������ݱ�����Ч...
	QString strQIPSend = m_ui.comIPSend->itemData(nCurIndex).toString();
	if (nCurIndex < 0 || strQIPSend.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.IPSend.Empty"));
		return;
	}
	// ��һ��ת����ȡ��������...
	ROLE_TYPE nRoleType = (ROLE_TYPE)m_ui.comRole->currentIndex();
	string strMainName = string((const char *)strQNameSet.toUtf8());
	string strIPSend = string((const char*)strQIPSend.toUtf8());
	CWebThread * lpWebThread = App()->GetWebThread();
	// ����վ��������������ʧ�ܣ�������ʾ��Ȼ�󷵻�...
	if (!lpWebThread->doWebSaveSys(nRoleType, strMainName, strIPSend)) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("SetSys.Post.Error"));
		return;
	}
	// ����ɹ�������ȫ������...
	App()->SetRoleType(nRoleType);
	App()->SetMainName(strMainName);
	App()->SetMultiIPSendAddr(strIPSend);
	// ���ýӿڣ��˳��Ի���...
	this->done(DialogCode::Accepted);
}

// ���ȡ����ť...
void CDlgSetSys::reject()
{
	this->done(DialogCode::Rejected);
}