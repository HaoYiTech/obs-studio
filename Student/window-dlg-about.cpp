
#include <QCloseEvent>
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-about.hpp"

CDlgAbout::CDlgAbout(QWidget *parent)
  : QDialog (parent)
{
	// ���ý������...
	m_ui.setupUi(this);
	// ���ñ������������ݣ����ô���ͼ��...
	this->setWindowTitle(QTStr("About.Title"));
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// �������������Ϣ => ��Ҫ��UTF8ת����Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	string & strMacMD5 = App()->GetAuthMacMD5();
	os_utf8_to_wcs(strMacMD5.c_str(), strMacMD5.size(), szWBuffer, MAX_PATH);
	m_ui.stickName->setStyleSheet("background-color:#f0f0f0;color:darkblue;");
	m_ui.stickName->setText(QString((QChar*)szWBuffer));
	m_ui.roleType->setText(App()->GetRoleString());
	// ��ʾ��Ȩ��Ϣ => ��Ȩ�汾���������������ڵȵ�...
	QString strAuthorizeText;
	int nAuthDays = App()->GetAuthDays();
	bool bAuthLicense = App()->GetAuthLicense();
	string & strAuthExpired = App()->GetAuthExpired();
	if (bAuthLicense) { strAuthorizeText = QTStr("About.Auth.UnLimit"); }
	else { strAuthorizeText = QTStr("About.Auth.Limited").arg(nAuthDays).arg(strAuthExpired.c_str()); }
	m_ui.authName->setText(strAuthorizeText);
	// ��ʾ�汾��Ϣ => ��ʾ�������ںͰ汾��Ϣ...
	m_ui.versionName->setText(QString("V%1 - Build %2").arg(SZ_VERSION_NAME).arg(__DATE__));
	// ��ʾ��վ��ַ...
	m_ui.webName->setText(DEF_WEB_CLASS);
}

CDlgAbout::~CDlgAbout()
{
}

// ���ȷ�ϰ�ť...
void CDlgAbout::accept()
{
	// ���ýӿڣ��˳��Ի���...
	this->done(DialogCode::Accepted);
}
