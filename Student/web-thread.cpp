
#include "student-app.h"
#include "web-thread.h"
#include "StringParser.h"
#include "SocketUtils.h"
#include <curl.h>

CWebThread::CWebThread()
  : m_eRegState(kRegHaoYi)
{
}

CWebThread::~CWebThread()
{
	this->StopAndWaitForThread();
}

GM_Error CWebThread::InitThread()
{
	this->Start();
	return GM_NoErr;
}
size_t procWebPostCurl(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CWebThread * lpThread = (CWebThread*)stream;
	lpThread->doPostCurl((char*)ptr, size * nmemb);
	return size * nmemb;
}
//
// 将每次反馈的数据进行累加 => 有可能一次请求的数据是分开发送的...
void CWebThread::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE输出有长度限制，太长会截断...
}
//
// 解析JSON数据头信息...
bool CWebThread::parseJson(Json::Value & outValue)
{
	// 判断获取的数据是否有效...
	if (m_strUTF8Data.size() <= 0) {
		MsgLogGM(GM_Err_Json);
		return false;
	}
	Json::Reader reader;
	string strANSIData = m_strUTF8Data;
	// 解析转换后的json数据包...
	if (!reader.parse(strANSIData, outValue)) {
		MsgLogGM(GM_Err_Json);
		return false;
	}
	// 获取返回的采集端编号和错误信息...
	if (outValue["err_code"].asBool()) {
		string & strMsg = outValue["err_msg"].asString();
		blog(LOG_INFO, "%s", strMsg.c_str());
		return false;
	}
	// 没有错误，返回正确...
	return true;
}

bool CWebThread::RegisterGather()
{
	// 先设置当前状态信息...
	m_eRegState = kRegGather;
	m_strUTF8Data.clear();
	// 判断数据是否有效...
	int nWebPort = App()->GetWebPort();
	string  & strWebAddr = App()->GetWebAddr();
	string & strMacAddr = App()->GetLocalMacAddr();
	string & strIPAddr = App()->GetLocalIPAddr();
	if (strWebAddr.size() <= 0 || nWebPort <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	if (strMacAddr.size() <= 0 || strIPAddr.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 准备需要的汇报数据 => POST数据包 => 都是UTF8格式...
	char  strPost[MAX_PATH] = { 0 };
	char  strUrl[MAX_PATH] = { 0 };
	char  szDNS[MAX_PATH] = { 0 };
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8Name = CStudentApp::GetServerDNSName();
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	sprintf(strPost, "mac_addr=%s&ip_addr=%s&name_pc=%s&os_name=%s", strMacAddr.c_str(), strIPAddr.c_str(), szDNS, CStudentApp::GetServerOS());
	sprintf(strUrl, "%s:%d/wxapi.php/Gather/index", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL)
			break;
		// 如果是https://协议，需要新增参数...
		/*if (theConfig.IsWebHttps()) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}*/
		// 设定curl参数，采用post模式，设置5秒超时...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(strPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procWebPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if (!this->parseJson(value)) {
		//m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthRegister, false);
		return false;
	}
	// 处理新增的配置内容 => name_set | web_name => 中间存放都是UTF8格式，显示时才转换成Unicode...
	string strMainName = CStudentApp::getJsonString(value["name_set"]).c_str();
	int nMainKbps = atoi(CStudentApp::getJsonString(value["main_rate"]).c_str());
	int nSubKbps = atoi(CStudentApp::getJsonString(value["sub_rate"]).c_str());
	int nSliceVal = atoi(CStudentApp::getJsonString(value["slice_val"]).c_str());
	int nInterVal = atoi(CStudentApp::getJsonString(value["inter_val"]).c_str());
	int nSnapVal = atoi(CStudentApp::getJsonString(value["snap_val"]).c_str());
	bool bAutoLinkDVR = atoi(CStudentApp::getJsonString(value["auto_dvr"]).c_str());
	bool bAutoLinkFDFS = atoi(CStudentApp::getJsonString(value["auto_fdfs"]).c_str());
	bool bAutoDetectIPC = atoi(CStudentApp::getJsonString(value["auto_ipc"]).c_str());
	int nPageSize = atoi(CStudentApp::getJsonString(value["page_size"]).c_str());
	// 对每页窗口数进行重新适配判断计算...
	//nPageSize = ((nPageSize <= 1 || nPageSize > 36) ? DEF_PER_PAGE_SIZE : nPageSize);
	//int nColNum = ceil(sqrt(nPageSize * 1.0f));
	//nPageSize = nColNum * nColNum;
	// 判断是否需要处理挂载的直播间信息...
	/*if (value.isMember("selected") && value.isMember("begin")) {
		int nCurSelRoomID = atoi(CStudentApp::getJsonString(value["selected"]).c_str());
		int nBeginID = atoi(CStudentApp::getJsonString(value["begin"]).c_str());
		theConfig.SetCurSelRoomID(nCurSelRoomID);
		theConfig.SetBeginRoomID(nBeginID);
	}*/
	// 获取Tracker|Remote|Local，并存放到配置文件，但不存盘...
	int nDBGatherID = atoi(CStudentApp::getJsonString(value["gather_id"]).c_str());
	int nWebType = atoi(CStudentApp::getJsonString(value["web_type"]).c_str());
	string strRemoteAddr = CStudentApp::getJsonString(value["transmit_addr"]);
	int nRemotePort = atoi(CStudentApp::getJsonString(value["transmit_port"]).c_str());
	string strTrackerAddr = CStudentApp::getJsonString(value["tracker_addr"]);
	int nTrackerPort = atoi(CStudentApp::getJsonString(value["tracker_port"]).c_str());
	string strWebName = CStudentApp::getJsonString(value["web_name"]).c_str();
	string strWebTag = CStudentApp::getJsonString(value["web_tag"]);
	string strWebVer = CStudentApp::getJsonString(value["web_ver"]);
	// 获取通道记录列表，先清除旧的列表...
	GM_MapNodeCamera & theNode = App()->GetNodeCamera();
	Json::Value & theCamera = value["camera"];
	theNode.clear();
	// 解析通道记录列表...
	if (theCamera.isArray()) {
		for (int i = 0; i < theCamera.size(); ++i) {
			int nDBCameraID; GM_MapData theMapData;
			theMapData["camera_id"] = CStudentApp::getJsonString(theCamera[i]["camera_id"]);
			nDBCameraID = atoi(theMapData["camera_id"].c_str());
			theNode[nDBCameraID] = theMapData;
		}
	}
	// 通知主窗口授权网站注册结果...
	/*m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthRegister, ((nDBGatherID > 0) ? true : false));
	// 判断采集端是否注册成功...
	if (nDBGatherID <= 0 || strWebTag.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	ASSERT(nDBGatherID > 0 && strWebTag.size() > 0);
	if (nWebType < 0 || strWebName.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 判断Tracker地址是否已经正确获取得到...
	if (strTrackerAddr.size() <= 0 || nTrackerPort <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	if (strRemoteAddr.size() <= 0 || nRemotePort <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 录像切片、切片交错，可以为0，0表示不切片，不交错...
	if (nInterVal < 0 || nSliceVal < 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 主码流和子码流必须有效...
	if (nMainKbps <= 0 || nSubKbps <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}*/
	// 设置默认的截图时间间隔 => 不要超过10分钟...
	nSnapVal = ((nSnapVal <= 0) ? 2 : nSnapVal);
	nSnapVal = ((nSnapVal >= 10) ? 10 : nSnapVal);
	// 存放到配置文件，但并不存盘 => 字符串都是UTF8格式...
	App()->SetDBGatherID(nDBGatherID);
	App()->SetWebVer(strWebVer);
	App()->SetWebTag(strWebTag);
	App()->SetWebType(nWebType);
	App()->SetWebName(strWebName);
	App()->SetRemoteAddr(strRemoteAddr);
	App()->SetRemotePort(nRemotePort);
	App()->SetTrackerAddr(strTrackerAddr);
	App()->SetTrackerPort(nTrackerPort);
	// 存放新增的采集端配置信息 => 字符串都是UTF8格式...
	App()->SetMainName(strMainName);
	App()->SetInterVal(nInterVal);
	App()->SetSliceVal(nSliceVal);
	App()->SetSnapVal(nSnapVal);
	App()->SetAutoLinkFDFS(bAutoLinkFDFS);
	App()->SetAutoLinkDVR(bAutoLinkDVR);
	// 注意：已经获取了通道编号列表...
	return true;
}

bool CWebThread::RegisterHaoYi()
{
	// 先设置当前状态信息...
	m_eRegState = kRegHaoYi;
	m_strUTF8Data.clear();
	// 判断数据是否有效...
	int nWebType = App()->GetWebType();
	int nWebPort = App()->GetWebPort();
	string  & strMainName = App()->GetMainName();
	string  & strWebVer = App()->GetWebVer();
	string  & strWebTag = App()->GetWebTag();
	string  & strWebName = App()->GetWebName();
	string  & strWebAddr = App()->GetWebAddr();
	string  & strMacAddr = App()->GetLocalMacAddr();
	string  & strIPAddr = App()->GetLocalIPAddr();
	string    strWebProto = "http";
	string    strOnlyAddr = strWebAddr.c_str() + strWebProto.size() + 3;
	if (strMacAddr.size() <= 0 || strIPAddr.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 网站节点标记不能为空 => 必须先通过RegisterGather过程...
	if (strMainName.size() <= 0 || strWebTag.size() <= 0 || nWebType < 0 || strWebName.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 准备需要的汇报数据 => POST数据包...
	char    strPost[MAX_PATH*3] = { 0 };
	char    strUrl[MAX_PATH] = { 0 };
	char	szDNS[MAX_PATH] = { 0 };
	char    szWebName[MAX_PATH] = { 0 };
	char    szMainName[MAX_PATH] = { 0 };
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8Name = CStudentApp::GetServerDNSName();
	string  strUTF8Main = strMainName;
	string	strUTF8Web = strWebName;
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	StringParser::EncodeURI(strUTF8Web.c_str(), strUTF8Web.size(), szWebName, MAX_PATH);
	StringParser::EncodeURI(strUTF8Main.c_str(), strUTF8Main.size(), szMainName, MAX_PATH);
	sprintf(strPost, "mac_addr=%s&ip_addr=%s&name_pc=%s&name_set=%s&version=%s&node_ver=%s&node_tag=%s&node_type=%d&node_addr=%s:%d&node_proto=%s&node_name=%s&os_name=%s",
		strMacAddr.c_str(), strIPAddr.c_str(), szDNS, szMainName, _T(SZ_VERSION_NAME), strWebVer.c_str(), strWebTag.c_str(), nWebType,
		strOnlyAddr.c_str(), nWebPort, strWebProto.c_str(), szWebName, CStudentApp::GetServerOS());
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	sprintf(strUrl, "%s/wxapi.php/Gather/verify", DEF_WEB_HOME);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL)
			break;
		// 设定curl参数，采用post模式，设置5秒超时，忽略证书检查...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(strPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procWebPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if (!this->parseJson(value)) {
		// 授权失败，仍然要获取最大通道、到期日期、剩余天数...
		int nMaxCameraNum = atoi(CStudentApp::getJsonString(value["max_camera"]).c_str());
		int nAuthDays = atoi(CStudentApp::getJsonString(value["auth_days"]).c_str());
		string strExpired = CStudentApp::getJsonString(value["auth_expired"]);
		// 存放到配置对象，以便关于对话框查看授权状态...
		App()->SetMaxCamera(nMaxCameraNum);
		App()->SetAuthExpired(strExpired);
		App()->SetAuthDays(nAuthDays);
		// 授权失败，通知界面层...
		//m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthExpired, false);
		return false;
	}
	// 解析JSON成功，进一步解析数据...
	bool bAuthLicense = atoi(CStudentApp::getJsonString(value["auth_license"]).c_str());
	int nMaxCameraNum = atoi(CStudentApp::getJsonString(value["max_camera"]).c_str());
	int nHaoYiGatherID = atoi(CStudentApp::getJsonString(value["gather_id"]).c_str());
	int nHaoYiNodeID = atoi(CStudentApp::getJsonString(value["node_id"]).c_str());
	int nHaoYiUserID = atoi(CStudentApp::getJsonString(value["user_id"]).c_str());
	int nAuthDays = atoi(CStudentApp::getJsonString(value["auth_days"]).c_str());
	string strExpired = CStudentApp::getJsonString(value["auth_expired"]);
	// 通知主窗口授权过期验证结果...
	//m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthExpired, ((nHaoYiGatherID > 0) ? true : false));
	// 存放到配置对象，返回授权验证结果...
	App()->SetAuthDays(nAuthDays);
	App()->SetAuthExpired(strExpired);
	App()->SetAuthLicense(bAuthLicense);
	App()->SetMaxCamera(nMaxCameraNum);
	App()->SetDBHaoYiUserID(nHaoYiUserID);
	App()->SetDBHaoYiNodeID(nHaoYiNodeID);
	App()->SetDBHaoYiGatherID(nHaoYiGatherID);
	return ((nHaoYiGatherID > 0) ? true : false);
}

bool CWebThread::LogoutHaoYi()
{
	return true;
}

bool CWebThread::LogoutGather()
{
	return true;
}

bool CWebThread::GetAllCameraData()
{
	return true;
}

void CWebThread::Entry()
{
	do {
		// 首先，在网站上注册采集端信息...
		if (!this->RegisterGather())
			break;
		// 然后，需要验证授权是否已经过期...
		if (!this->RegisterHaoYi())
			break;
		// 开始注册摄像头，这里只能注册已知的，新建的不能注册，因此，需要在新扫描出来的地方增加注册功能...
		if (!this->GetAllCameraData())
			break;
		// 主视图启动组播频道自动搜索线程，启动Tracker自动连接，中间视图创建等等...
		//m_lpHaoYiView->PostMessage(WM_WEB_LOAD_RESOURCE);
		// 设置已经成功连接服务器标志...
		//m_bIsLoadSuccess = true;
	} while (false);
	// 如果连接失败，需要退出已经连接的部分内容...
	//if (!m_bIsLoadSuccess) {
	//	this->doWebGatherLogout();
	//}
}