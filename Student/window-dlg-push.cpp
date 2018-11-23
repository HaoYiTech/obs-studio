
#include <QCloseEvent>
#include "student-app.h"
#include "qt-wrappers.hpp"
#include "window-dlg-push.hpp"
#include "md5.h"

CDlgPush::CDlgPush(QWidget *parent, int nDBCameraID)
  : QDialog (parent)
  , m_nDBCameraID(nDBCameraID)
{
	// �����Ƿ��Ǳ༭ģʽ...
	m_bEdit = ((m_nDBCameraID > 0) ? true : false);
	// ���ý������...
	m_ui.setupUi(this);
	// ������ر���...
	m_ui.streamMode->setChecked(true);
	m_ui.fileMode->setEnabled(false);
	m_ui.mp4FileName->setEnabled(false);
	m_ui.mp4FilePath->setEnabled(false);
	m_ui.btnChoose->setEnabled(false);
	m_ui.cameraName->setFocus();
	// ���ñ������������ݣ����ô���ͼ��...
	QString strTitle = QString("%1 - %2").arg(QTStr("Camera.Setting.Title")).arg(QTStr(m_bEdit ? "Camera.Setting.Mod" : "Camera.Setting.Add"));
	this->setWindowTitle(strTitle);
	this->setWindowIcon(QIcon(":/res/HaoYi.ico"));
	// ������Ǳ༭ģʽ��ֱ�ӷ���...
	if (!m_bEdit)
		return;
	// �༭ģʽ����ȡ�������...
	GM_MapData & theMapData = m_MapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	if (theMapData.size() <= 0)
		return;
	// ��ȡ��Ҫ����ת��������Ϣ...
	string & strUsingTCP = theMapData["use_tcp"];
	string & strCameraName = theMapData["camera_name"];
	string & strStreamMP4 = theMapData["stream_mp4"];
	string & strStreamUrl = theMapData["stream_url"];
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	bool bFileMode = ((nStreamProp == kStreamUrlLink) ? false : true);
	bool bUsingTCP = ((strUsingTCP.size() > 0) ? atoi(strUsingTCP.c_str()) : false);
	// �����Ƿ�ʹ��TCPģʽ...
	m_ui.useTCP->setChecked(bUsingTCP);
	// ����ͨ������ => ��Ҫ��UTF8ת����Unicode...
	WCHAR szWBuffer[MAX_PATH] = { 0 };
	os_utf8_to_wcs(strCameraName.c_str(), strCameraName.size(), szWBuffer, MAX_PATH);
	m_ui.cameraName->setText(QString((QChar*)szWBuffer));
	// ������ļ�ģʽ��ֱ�ӷ���...
	if (bFileMode)
		return;
	// �Ի�ȡ�������ӵ�ַ���и�ʽת������UTF8ת����Unicode...
	os_utf8_to_wcs(strStreamUrl.c_str(), strStreamUrl.size(), szWBuffer, MAX_PATH);
	m_ui.streamUrl->setText(QString((QChar*)szWBuffer));
	// 2018.11.23 - by jackey => ͨ������������״̬Ҳ�ܱ༭...
	// ���ͨ����������״̬(�������� �� ������) => ��ֹ�޸�...
	/*if (!bIsOffLine) {
		m_ui.streamUrl->setReadOnly(true);
		m_ui.cameraName->setReadOnly(true);
		m_ui.streamMode->setEnabled(false);
		m_ui.useTCP->setEnabled(false);
		// �����е�ͨ�����ܽ���ͨ������...
		m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	}*/
}

CDlgPush::~CDlgPush()
{
}

// ���ȷ�ϰ�ť...
void CDlgPush::accept()
{
	// ��֤�������ݵ���Ч��...
	QString & strQCameraName = m_ui.cameraName->text();
	QString & strQStreamUrl = m_ui.streamUrl->text();
	if (strQCameraName.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Name.Empty"));
		m_ui.cameraName->setFocus();
		return;
	}
	// ������ַ����Ϊ��...
	if (strQStreamUrl.length() <= 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Url.Empty"));
		m_ui.streamUrl->setFocus();
		return;
	}
	// ������ַ�ĸ�ʽ��֤ => ��֤��ʽͷ��Ϣ...
	string strUTF8Name = string((const char *)strQCameraName.toUtf8());
	string strUTF8Url = string((const char *)strQStreamUrl.toUtf8());
	if (strnicmp("rtsp://", strUTF8Url.c_str(), strlen("rtsp://")) != 0) {
		OBSMessageBox::information(this, QTStr("Student.Error.Title"), QTStr("Student.Url.Error"));
		m_ui.streamUrl->setFocus();
		return;
	}
	// ���������ݴ�ŵ����ü��϶��е���...
	int  nStreamProp = (m_ui.streamMode->isChecked() ? kStreamUrlLink : kStreamMP4File);
	bool bUseTCP = m_ui.useTCP->isChecked();
	char szBuffer[MAX_PATH] = { 0 };
	m_MapData["camera_name"] = strUTF8Name;
	m_MapData["stream_url"] = strUTF8Url;
	sprintf(szBuffer, "%d", nStreamProp);
	m_MapData["stream_prop"] = szBuffer;
	sprintf(szBuffer, "%d", bUseTCP);
	m_MapData["use_tcp"] = szBuffer;
	// ���������Ӳ���������device_sn...
	if (!m_bEdit) {
		MD5	 md5;
		ULARGE_INTEGER	llTimCountCur = { 0 };
		::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
		sprintf(szBuffer, "%I64d", llTimCountCur.QuadPart);
		md5.update(szBuffer, strlen(szBuffer));
		m_MapData["device_sn"] = md5.toString();
	}
	// ���ýӿڣ��˳��Ի���...
	this->done(DialogCode::Accepted);
}

// ���ȡ����ť...
void CDlgPush::reject()
{
	this->done(DialogCode::Rejected);
}