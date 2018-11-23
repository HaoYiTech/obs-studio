
#include <QCloseEvent>
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-push.hpp"
#include "md5.h"

CDlgPush::CDlgPush(QWidget *parent, int nDBCameraID)
  : QDialog (parent)
  , m_nDBCameraID(nDBCameraID)
{
	// 计算是否是编辑模式...
	m_bEdit = ((m_nDBCameraID > 0) ? true : false);
	// 设置界面对象...
	m_ui.setupUi(this);
	// 设置相关变量...
	m_ui.streamMode->setChecked(true);
	m_ui.fileMode->setEnabled(false);
	m_ui.mp4FileName->setEnabled(false);
	m_ui.mp4FilePath->setEnabled(false);
	m_ui.btnChoose->setEnabled(false);
	m_ui.cameraName->setFocus();
	// 设置标题栏文字内容，设置窗口图标...
	QString strTitle = QString("%1 - %2").arg(QTStr("Camera.Setting.Title")).arg(QTStr(m_bEdit ? "Camera.Setting.Mod" : "Camera.Setting.Add"));
	this->setWindowTitle(strTitle);
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// 如果不是编辑模式，直接返回...
	if (!m_bEdit)
		return;
	// 编辑模式，获取填充内容...
	GM_MapData & theMapData = m_MapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	if (theMapData.size() <= 0)
		return;
	// 获取需要的流转发参数信息...
	string & strUsingTCP = theMapData["use_tcp"];
	string & strCameraName = theMapData["camera_name"];
	string & strStreamMP4 = theMapData["stream_mp4"];
	string & strStreamUrl = theMapData["stream_url"];
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	bool bFileMode = ((nStreamProp == kStreamUrlLink) ? false : true);
	bool bUsingTCP = ((strUsingTCP.size() > 0) ? atoi(strUsingTCP.c_str()) : false);
	// 设置是否使用TCP模式...
	m_ui.useTCP->setChecked(bUsingTCP);
	// 设置通道名称 => 需要从UTF8转换成Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	os_utf8_to_wcs(strCameraName.c_str(), strCameraName.size(), szWBuffer, MAX_PATH);
	m_ui.cameraName->setText(QString((QChar*)szWBuffer));
	// 如果是文件模式，直接返回...
	if (bFileMode)
		return;
	// 对获取到的链接地址进行格式转换，从UTF8转换成Unicode...
	os_utf8_to_wcs(strStreamUrl.c_str(), strStreamUrl.size(), szWBuffer, MAX_PATH);
	m_ui.streamUrl->setText(QString((QChar*)szWBuffer));
	// 2018.11.23 - by jackey => 通道处于已连接状态也能编辑...
	// 如果通道处于运行状态(正在连接 或 已连接) => 禁止修改...
	/*if (!bIsOffLine) {
		m_ui.streamUrl->setReadOnly(true);
		m_ui.cameraName->setReadOnly(true);
		m_ui.streamMode->setEnabled(false);
		m_ui.useTCP->setEnabled(false);
		// 运行中的通道不能进行通道配置...
		m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	}*/
}

CDlgPush::~CDlgPush()
{
}

// 点击确认按钮...
void CDlgPush::accept()
{
	// 验证输入数据的有效性...
	QString & strQCameraName = m_ui.cameraName->text();
	QString & strQStreamUrl = m_ui.streamUrl->text();
	if (strQCameraName.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Name.Empty"));
		m_ui.cameraName->setFocus();
		return;
	}
	// 拉流地址不能为空...
	if (strQStreamUrl.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Url.Empty"));
		m_ui.streamUrl->setFocus();
		return;
	}
	// 拉流地址的格式验证 => 验证格式头信息...
	string strUTF8Name = string((const char *)strQCameraName.toUtf8());
	string strUTF8Url = string((const char *)strQStreamUrl.toUtf8());
	if (strnicmp("rtsp://", strUTF8Url.c_str(), strlen("rtsp://")) != 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Url.Error"));
		m_ui.streamUrl->setFocus();
		return;
	}
	// 将最终数据存放到配置集合队列当中...
	int  nStreamProp = (m_ui.streamMode->isChecked() ? kStreamUrlLink : kStreamMP4File);
	bool bUseTCP = m_ui.useTCP->isChecked();
	char szBuffer[MAX_PATH] = { 0 };
	m_MapData["camera_name"] = strUTF8Name;
	m_MapData["stream_url"] = strUTF8Url;
	sprintf(szBuffer, "%d", nStreamProp);
	m_MapData["stream_prop"] = szBuffer;
	sprintf(szBuffer, "%d", bUseTCP);
	m_MapData["use_tcp"] = szBuffer;
	// 如果是新添加操作，新增device_sn...
	if (!m_bEdit) {
		MD5	 md5;
		ULARGE_INTEGER	llTimCountCur = { 0 };
		::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
		sprintf(szBuffer, "%I64d", llTimCountCur.QuadPart);
		md5.update(szBuffer, strlen(szBuffer));
		m_MapData["device_sn"] = md5.toString();
	}
	// 调用接口，退出对话框...
	this->done(DialogCode::Accepted);
}

// 点击取消按钮...
void CDlgPush::reject()
{
	this->done(DialogCode::Rejected);
}