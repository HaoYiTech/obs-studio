
#include "student-app.h"

CStudentApp::CStudentApp(int &argc, char **argv)
  : QApplication(argc, argv)
{
}

CStudentApp::~CStudentApp()
{
}

// 创建并初始化登录窗口...
void CStudentApp::doLoginInit()
{
	// 创建登录窗口，显示登录窗口...
	m_loginWindow = new LoginWindow();
	m_loginWindow->show();
	// 建立登录窗口与应用对象的信号槽关联函数...
	connect(m_loginWindow, SIGNAL(loginSuccess()), this, SLOT(doLoginSuccess()));
}

// 处理登录成功之后的信号槽事件...
void CStudentApp::doLoginSuccess()
{
	// 先关闭登录窗口...
	m_loginWindow->close();
}

// 处理学生端退出事件通知...
void CStudentApp::doLogoutEvent()
{
	// 释放存储会话对象...
	/*if (m_TrackerSession != NULL) {
	delete m_TrackerSession;
	m_TrackerSession = NULL;
	}
	if (m_StorageSession != NULL) {
	delete m_StorageSession;
	m_StorageSession = NULL;
	}
	// 获取云教室号码的配置对象...
	const char * lpLiveRoomID = config_get_string(this->GlobalConfig(), "General", "LiveRoomID");
	if (lpLiveRoomID == NULL)
	return;
	// 准备登出需要的云教室号码缓冲区...
	char   szPost[MAX_PATH] = { 0 };
	char * lpStrUrl = "http://edu.ihaoyi.cn/wxapi.php/Gather/logoutLiveRoom";
	sprintf(szPost, "room_id=%s", lpLiveRoomID);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
	if (curl == NULL)
	break;
	// 设定curl参数，采用post模式，设置5秒超时...
	res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPost);
	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPost));
	res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
	res = curl_easy_setopt(curl, CURLOPT_POST, true);
	res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
	res = curl_easy_setopt(curl, CURLOPT_URL, lpStrUrl);
	res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
	curl_easy_cleanup(curl);
	}*/
}