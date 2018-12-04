
#include <QCloseEvent>
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-about.hpp"

CDlgAbout::CDlgAbout(QWidget *parent)
  : QDialog (parent)
{
	// 设置界面对象...
	m_ui.setupUi(this);
	// 设置标题栏文字内容，设置窗口图标...
	this->setWindowTitle(QTStr("About.Title"));
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// 设置相关内容信息 => 需要从UTF8转换成Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	string & strMacMD5 = App()->GetAuthMacMD5();
	os_utf8_to_wcs(strMacMD5.c_str(), strMacMD5.size(), szWBuffer, MAX_PATH);
	m_ui.stickName->setStyleSheet("background-color:#f0f0f0;color:darkblue;");
	m_ui.stickName->setText(QString((QChar*)szWBuffer));
	m_ui.roleType->setText(App()->GetRoleString());
	// 显示授权信息 => 授权版本、天数、过期日期等等...
	QString strAuthorizeText;
	int nAuthDays = App()->GetAuthDays();
	bool bAuthLicense = App()->GetAuthLicense();
	string & strAuthExpired = App()->GetAuthExpired();
	if (bAuthLicense) { strAuthorizeText = QTStr("About.Auth.UnLimit"); }
	else { strAuthorizeText = QTStr("About.Auth.Limited").arg(nAuthDays).arg(strAuthExpired.c_str()); }
	m_ui.authName->setText(strAuthorizeText);
	// 显示版本信息 => 显示编译日期和版本信息...
	m_ui.versionName->setText(QString("V%1 - Build %2").arg(SZ_VERSION_NAME).arg(__DATE__));
	// 显示网站地址...
	m_ui.webName->setText(DEF_WEB_CLASS);
}

CDlgAbout::~CDlgAbout()
{
}

// 点击确认按钮...
void CDlgAbout::accept()
{
	// 调用接口，退出对话框...
	this->done(DialogCode::Accepted);
}
