
#include "student-app.h"

CStudentApp::CStudentApp(int &argc, char **argv)
  : QApplication(argc, argv)
{
}

CStudentApp::~CStudentApp()
{
}

// ��������ʼ����¼����...
void CStudentApp::doLoginInit()
{
	// ������¼���ڣ���ʾ��¼����...
	m_loginWindow = new LoginWindow();
	m_loginWindow->show();
	// ������¼������Ӧ�ö�����źŲ۹�������...
	connect(m_loginWindow, SIGNAL(loginSuccess()), this, SLOT(doLoginSuccess()));
}

// �����¼�ɹ�֮����źŲ��¼�...
void CStudentApp::doLoginSuccess()
{
	// �ȹرյ�¼����...
	m_loginWindow->close();
}

// ����ѧ�����˳��¼�֪ͨ...
void CStudentApp::doLogoutEvent()
{
	// �ͷŴ洢�Ự����...
	/*if (m_TrackerSession != NULL) {
	delete m_TrackerSession;
	m_TrackerSession = NULL;
	}
	if (m_StorageSession != NULL) {
	delete m_StorageSession;
	m_StorageSession = NULL;
	}
	// ��ȡ�ƽ��Һ�������ö���...
	const char * lpLiveRoomID = config_get_string(this->GlobalConfig(), "General", "LiveRoomID");
	if (lpLiveRoomID == NULL)
	return;
	// ׼���ǳ���Ҫ���ƽ��Һ��뻺����...
	char   szPost[MAX_PATH] = { 0 };
	char * lpStrUrl = "http://edu.ihaoyi.cn/wxapi.php/Gather/logoutLiveRoom";
	sprintf(szPost, "room_id=%s", lpLiveRoomID);
	// ����Curl�ӿڣ��㱨�ɼ�����Ϣ...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
	if (curl == NULL)
	break;
	// �趨curl����������postģʽ������5�볬ʱ...
	res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
	res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
	res = curl_easy_setopt(curl, CURLOPT_POST, true);
	res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
	res = curl_easy_setopt(curl, CURLOPT_URL, lpStrUrl);
	res = curl_easy_perform(curl);
	} while (false);
	// �ͷ���Դ...
	if (curl != NULL) {
	curl_easy_cleanup(curl);
	}*/
}