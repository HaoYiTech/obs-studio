
#include "student-app.h"
#include "web-thread.h"
#include "StringParser.h"
#include "SocketUtils.h"
#include <curl.h>

CWebThread::CWebThread()
  : m_bIsCanReConnect(false)
  , m_bIsLoadSuccess(false)
  , m_eRegState(kRegHaoYi)
  , m_nCurCameraCount(0)
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
	string & strWebClass = App()->GetWebClass();
	string & strMacAddr = App()->GetLocalMacAddr();
	string & strIPAddr = App()->GetLocalIPAddr();
	if (strWebClass.size() <= 0 || strMacAddr.size() <= 0 || strIPAddr.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 准备需要的汇报数据 => POST数据包 => 都是UTF8格式...
	char  strPost[MAX_PATH] = { 0 };
	char  strUrl[MAX_PATH] = { 0 };
	char  szDNS[MAX_PATH] = { 0 };
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8DNS = CStudentApp::GetServerDNSName();
	StringParser::EncodeURI(strUTF8DNS.c_str(), strUTF8DNS.size(), szDNS, MAX_PATH);
	sprintf(strPost, "mac_addr=%s&ip_addr=%s&name_pc=%s&os_name=%s", strMacAddr.c_str(), strIPAddr.c_str(), szDNS, CStudentApp::GetServerOS());
	sprintf(strUrl, "%s/wxapi.php/Gather/index", strWebClass.c_str());
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL)
			break;
		// 如果是https://协议，需要新增参数...
		if (App()->IsClassHttps()) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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
	// 解析JSON失败，通知界面层 => 通过信号槽代替消息异步发送...
	if (!this->parseJson(value)) {
		emit App()->msgFromWebThread(WM_WEB_AUTH_RESULT, kAuthRegister, false);
		return false;
	}
	// 处理新增的配置内容 => name_set | web_name => 中间存放都是UTF8格式，显示时才转换成Unicode...
	string strMainName = CStudentApp::getJsonString(value["name_set"]);
	int nMainKbps = atoi(CStudentApp::getJsonString(value["main_rate"]).c_str());
	int nSubKbps = atoi(CStudentApp::getJsonString(value["sub_rate"]).c_str());
	int nSliceVal = atoi(CStudentApp::getJsonString(value["slice_val"]).c_str());
	int nInterVal = atoi(CStudentApp::getJsonString(value["inter_val"]).c_str());
	int nSnapVal = atoi(CStudentApp::getJsonString(value["snap_val"]).c_str());
	bool bAutoLinkDVR = atoi(CStudentApp::getJsonString(value["auto_dvr"]).c_str());
	bool bAutoLinkFDFS = atoi(CStudentApp::getJsonString(value["auto_fdfs"]).c_str());
	bool bAutoDetectIPC = atoi(CStudentApp::getJsonString(value["auto_ipc"]).c_str());
	int nPageSize = atoi(CStudentApp::getJsonString(value["page_size"]).c_str());
	ROLE_TYPE nRoleType = (ROLE_TYPE)atoi(CStudentApp::getJsonString(value["role_type"]).c_str());
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
	string strWebName = CStudentApp::getJsonString(value["web_name"]);
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
	// 设置默认的截图时间间隔 => 不要超过10分钟...
	nSnapVal = ((nSnapVal <= 0) ? 2 : nSnapVal);
	nSnapVal = ((nSnapVal >= 10) ? 10 : nSnapVal);
	// 存放到配置文件，但并不存盘 => 字符串都是UTF8格式...
	App()->SetDBGatherID(nDBGatherID);
	App()->SetRoleType(nRoleType);
	App()->SetWebVer(strWebVer);
	App()->SetWebTag(strWebTag);
	App()->SetWebType(nWebType);
	App()->SetWebName(strWebName);
	// 存放新增的采集端配置信息 => 字符串都是UTF8格式...
	App()->SetMainName(strMainName);
	App()->SetInterVal(nInterVal);
	App()->SetSliceVal(nSliceVal);
	App()->SetSnapVal(nSnapVal);
	App()->SetAutoLinkFDFS(bAutoLinkFDFS);
	App()->SetAutoLinkDVR(bAutoLinkDVR);
	// 注意：已经获取了通道编号列表...
	// 通知主窗口授权网站注册结果 => 通过信号槽代替消息异步发送...
	emit App()->msgFromWebThread(WM_WEB_AUTH_RESULT, kAuthRegister, ((nDBGatherID > 0) ? true : false));
	return true;
}

bool CWebThread::RegisterHaoYi()
{
	// 先设置当前状态信息...
	m_eRegState = kRegHaoYi;
	m_strUTF8Data.clear();
	// 判断数据是否有效...
	int nWebType = App()->GetWebType();
	int nWebPort = App()->IsClassHttps() ? DEF_SSL_PORT : DEF_WEB_PORT;
	string    strWebProto = App()->IsClassHttps() ? DEF_SSL_PROTO : DEF_WEB_PROTO;
	string  & strWebClass = App()->GetWebClass();
	string  & strWebVer = App()->GetWebVer();
	string  & strWebTag = App()->GetWebTag();
	string  & strWebName = App()->GetWebName();
	string  & strMacAddr = App()->GetLocalMacAddr();
	string  & strIPAddr = App()->GetLocalIPAddr();
	string  & strMainName = App()->GetMainName();
	string    strOnlyAddr = strWebClass.c_str() + strWebProto.size() + 3;
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
	string  strUTF8DNS = CStudentApp::GetServerDNSName();
	StringParser::EncodeURI(strUTF8DNS.c_str(), strUTF8DNS.size(), szDNS, MAX_PATH);
	StringParser::EncodeURI(strWebName.c_str(), strWebName.size(), szWebName, MAX_PATH);
	StringParser::EncodeURI(strMainName.c_str(), strMainName.size(), szMainName, MAX_PATH);
	sprintf(strPost, "mac_addr=%s&ip_addr=%s&name_pc=%s&name_set=%s&version=%s&node_ver=%s&node_tag=%s&node_type=%d&node_addr=%s:%d&node_proto=%s&node_name=%s&os_name=%s",
		strMacAddr.c_str(), strIPAddr.c_str(), szDNS, szMainName, _T(SZ_VERSION_NAME), strWebVer.c_str(), strWebTag.c_str(), nWebType,
		strOnlyAddr.c_str(), nWebPort, strWebProto.c_str(), szWebName, CStudentApp::GetServerOS());
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	sprintf(strUrl, "%s/wxapi.php/Gather/verify", App()->GetWebCenter().c_str());
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
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
		// 授权失败，仍然要获取最大通道、到期日期、剩余天数...
		int nMaxCameraNum = atoi(CStudentApp::getJsonString(value["max_camera"]).c_str());
		int nAuthDays = atoi(CStudentApp::getJsonString(value["auth_days"]).c_str());
		string strExpired = CStudentApp::getJsonString(value["auth_expired"]);
		// 存放到配置对象，以便关于对话框查看授权状态...
		App()->SetMaxCamera(nMaxCameraNum);
		App()->SetAuthExpired(strExpired);
		App()->SetAuthDays(nAuthDays);
		// 授权失败，通知界面层 => 通过信号槽代替消息异步发送...
		emit App()->msgFromWebThread(WM_WEB_AUTH_RESULT, kAuthExpired, false);
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
	// 通知主窗口授权过期验证结果 => 通过信号槽代替消息异步发送...
	emit App()->msgFromWebThread(WM_WEB_AUTH_RESULT, kAuthExpired, ((nHaoYiGatherID > 0) ? true : false));
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

bool CWebThread::GetAllCameraData()
{
	GM_MapNodeCamera & theMapCamera = App()->GetNodeCamera();
	GM_MapNodeCamera::iterator itorItem = theMapCamera.begin();
	while (itorItem != theMapCamera.end()) {
		// 已注册摄像头数目不够，进行网站注册操作...
		int nDBCameraID = itorItem->first;
		GM_MapData & theData = itorItem->second;
		if (!this->doWebGetCamera(nDBCameraID)) {
			// 注册摄像头失败，删除摄像头配置...
			theMapCamera.erase(itorItem++);
		} else {
			// 注册摄像头成功，累加配置...
			++itorItem;
		}
	}
	return true;
}
//
// 采集端从中心退出...
bool CWebThread::LogoutHaoYi()
{
	// 获取网站配置信息 => GatherID是中心服务器中的编号...
	int nDBHaoYiGatherID = App()->GetDBHaoYiGatherID();
	if (nDBHaoYiGatherID <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGatherLogout;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char strPost[MAX_PATH] = { 0 };
	char strUrl[MAX_PATH] = { 0 };
	sprintf(strPost, "gather_id=%d", nDBHaoYiGatherID);
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	sprintf(strUrl, "%s/wxapi.php/Gather/logout", App()->GetWebCenter().c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(strPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要处理网站返回的数据...
		//res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		//res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	// 需要将配置文件中服务器编号置空...
	App()->SetDBHaoYiGatherID(-1);
	App()->SetDBHaoYiNodeID(-1);
	return true;
}
//
// 采集端从节点退出...
bool CWebThread::LogoutGather()
{
	// 获取网站配置信息...
	int nDBGatherID = App()->GetDBGatherID();
	string & strWebClass = App()->GetWebClass();
	if (nDBGatherID <= 0 || strWebClass.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGatherLogout;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char strPost[MAX_PATH] = { 0 };
	char strUrl[MAX_PATH] = { 0 };
	sprintf(strPost, "gather_id=%d", nDBGatherID);
	// 组合访问链接地址...
	sprintf(strUrl, "%s/wxapi.php/Gather/logout", strWebClass.c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if (App()->IsClassHttps()) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(strPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要处理网站返回的数据...
		//res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		//res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	// 需要将配置文件中采集端编号置空...
	App()->SetDBGatherID(-1);
	return true;
}

// 从网站获取通道配置和通道下的录像配置...
bool CWebThread::doWebGetCamera(int nDBCameraID)
{
	// 获取网站配置信息...
	int nDBGatherID = App()->GetDBGatherID();
	string & strWebClass = App()->GetWebClass();
	if ( strWebClass.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 如果当前已注册摄像头数目超过了最大支持数，不用再注册...
	if (m_nCurCameraCount >= App()->GetMaxCamera()) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGetCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char strPost[MAX_PATH] = { 0 };
	char strUrl[MAX_PATH] = { 0 };
	sprintf(strPost, "gather_id=%d&camera_id=%d", nDBGatherID, nDBCameraID);
	sprintf(strUrl, "%s/wxapi.php/Gather/getCamera", strWebClass.c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
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
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 初始化数据库里的通道配置...
	GM_MapData    dbMapCamera;
	Json::Value & theDBCamera = value["camera"];
	if (theDBCamera.isObject()) {
		// 算子itorItem必须放在内部定义，否则，会出现越界问题...
		for (Json::Value::iterator itorItem = theDBCamera.begin(); itorItem != theDBCamera.end(); ++itorItem) {
			const char * theKey = itorItem.memberName();
			// 包含中文的Key本身就是UTF8格式转换，只有在显示时才转换成QString格式...
			if (stricmp(theKey, "stream_url") == 0 || stricmp(theKey, "stream_mp4") == 0 ||
				stricmp(theKey, "camera_name") == 0 || stricmp(theKey, "device_user") == 0) {
				dbMapCamera[theKey] = CStudentApp::getJsonString(theDBCamera[theKey]);
			}
			else {
				dbMapCamera[theKey] = CStudentApp::getJsonString(theDBCamera[theKey]);
			}
		}
		//Json::Value::Members arrayMember = theDBCamera.getMemberNames();
	}
	// 获取录像课程表...
	/*GM_MapCourse  dbMapCourse;
	Json::Value & theCourse = value["course"];
	if (theCourse.isArray()) {
		for (int i = 0; i < theCourse.size(); ++i) {
			int nCourseID; GM_MapData theMapData;
			for (Json::Value::iterator itorItem = theCourse[i].begin(); itorItem != theCourse[i].end(); ++itorItem) {
				const char * theKey = itorItem.memberName();
				theMapData[theKey] = CStudentApp::getJsonString(theCourse[i][theKey]);
			}
			// 获取记录编号，存放到集合当中...
			nCourseID = atoi(theMapData["course_id"].c_str());
			dbMapCourse[nCourseID] = theMapData;
		}
	}
	// 将获取得到的录像课程表存放起来，直接覆盖原来的记录，用数据库编号定位...
	// 录像课程表都是记录到内存当中，不存入配置文件当中...
	if (dbMapCourse.size() > 0) {
		App()->SetCourse(nDBCameraID, dbMapCourse);
	}*/
	// 判断摄像头是否注册成功...
	if (dbMapCamera.size() <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 将获取到的通道配置，直接存放到内存当中，用数据库编号定位...
	App()->SetCamera(nDBCameraID, dbMapCamera);
	// 注册摄像头成功，摄像头累加计数...
	++m_nCurCameraCount;
	return true;
}

// 向网站删除摄像头...
bool CWebThread::doWebDelCamera(string & inDeviceSN)
{
	// 获取网站配置信息...
	string & strWebClass = App()->GetWebClass();
	if ( strWebClass.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	int nDBCameraID = -1;
	m_eRegState = kDelCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char strPost[MAX_PATH] = { 0 };
	char strUrl[MAX_PATH] = { 0 };
	sprintf(strPost, "device_sn=%s", inDeviceSN.c_str());
	// 组合访问链接地址...
	sprintf(strUrl, "%s/wxapi.php/Gather/delCamera", strWebClass.c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
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
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取返回的已删除的摄像头在数据库中的编号...
	nDBCameraID = atoi(CStudentApp::getJsonString(value["camera_id"]).c_str());
	// 判断摄像头是否删除成功...
	if (nDBCameraID <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	ASSERT(nDBCameraID > 0);
	// 摄像头计数器减少...
	m_nCurCameraCount -= 1;
	return true;
}

// 在网站上具体执行添加或更新摄像头操作...
bool CWebThread::doWebRegCamera(GM_MapData & inData)
{
	// 输入数据中必须包含device_sn和stream_prop字段...
	GM_MapData::iterator itorSN, itorProp;
	itorProp = inData.find("stream_prop");
	itorSN = inData.find("device_sn");
	if (itorSN == inData.end() || itorProp == inData.end()) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取网站配置信息...
	int nStreamProp = atoi(itorProp->second.c_str());
	int nDBGatherID = App()->GetDBGatherID();
	string & strWebClass = App()->GetWebClass();
	if ( strWebClass.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 如果当前已注册摄像头数目超过了最大支持数，不用再注册...
	if (m_nCurCameraCount >= App()->GetMaxCamera()) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kRegCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char  strPost[MAX_PATH*6] = { 0 };
	char  strUrl[MAX_PATH] = { 0 };
	char  szEncName[MAX_PATH] = { 0 };
	// 先对频道名称进行URI编码，本身就是UTF8编码...
	string strUTF8Name = inData["camera_name"];
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szEncName, MAX_PATH);
	// 目前只处理非设备型连接 => RTSP/RTMP/MP4...
	if (nStreamProp != kStreamDevice) {
		// 对需要的数据进行编码处理 => 这里需要注意文件名过长时的内存溢出问题...
		char   szMP4File[MAX_PATH * 3] = { 0 };
		char   szUrlLink[MAX_PATH * 2] = { 0 };
		string strUTF8MP4 = inData["stream_mp4"];
		string strUTF8Url = inData["stream_url"];
		StringParser::EncodeURI(strUTF8MP4.c_str(), strUTF8MP4.size(), szMP4File, MAX_PATH * 3);
		StringParser::EncodeURI(strUTF8Url.c_str(), strUTF8Url.size(), szUrlLink, MAX_PATH * 2);
		// 处理通道是流转发的情况...
		ASSERT(nStreamProp == kStreamMP4File || nStreamProp == kStreamUrlLink);
		sprintf(strPost, "gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&stream_mp4=%s&stream_url=%s&use_tcp=%s",
			nDBGatherID, nStreamProp, inData["camera_type"].c_str(), szEncName, inData["device_sn"].c_str(),
			szMP4File, szUrlLink, inData["use_tcp"].c_str());
	} /*else {
		// 对user和pass进行编码处理...
		TCHAR szDeviceUser[MAX_PATH] = { 0 };
		TCHAR szDevicePass[MAX_PATH] = { 0 };
		string strUTF8User = CUtilTool::ANSI_UTF8(inData["device_user"].c_str());
		string strUTF8Pass = CUtilTool::ANSI_UTF8(inData["device_pass"].c_str());
		StringParser::EncodeURI(strUTF8User.c_str(), strUTF8User.size(), szDeviceUser, MAX_PATH);
		StringParser::EncodeURI(strUTF8Pass.c_str(), strUTF8Pass.size(), szDevicePass, MAX_PATH);
		// 处理通道是摄像头的情况...
		strPost.Format("gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&device_ip=%s&device_mac=%s&device_type=%s&device_user=%s&"
			"device_pass=%s&device_cmd_port=%s&device_http_port=%s&device_mirror=%s&device_osd=%s&device_desc=%s&device_twice=%s&device_boot=%s&use_tcp=%s",
			nDBGatherID, nStreamProp, inData["camera_type"].c_str(), szEncName, inData["device_sn"].c_str(),
			inData["device_ip"].c_str(), inData["device_mac"].c_str(), inData["device_type"].c_str(),
			szDeviceUser, szDevicePass, inData["device_cmd_port"].c_str(), inData["device_http_port"].c_str(),
			inData["device_mirror"].c_str(), inData["device_osd"].c_str(), inData["device_desc"].c_str(),
			inData["device_twice"].c_str(), inData["device_boot"].c_str(), inData["use_tcp"].c_str());
	}*/
	// 组合访问链接地址...
	sprintf(strUrl, "%s/wxapi.php/Gather/regCamera", strWebClass.c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
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
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取已更新的通道在数据库中的编号...
	string strDBCamera = CStudentApp::getJsonString(value["camera_id"]);
	int nDBCameraID = atoi(strDBCamera.c_str());
	// 判断摄像头是否删除成功...
	if (nDBCameraID <= 0) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 将更新后的通道信息写入集合当中 => 添加或修改...
	ASSERT(nDBCameraID > 0);
	inData["camera_id"] = strDBCamera;
	App()->SetCamera(nDBCameraID, inData);
	return true;
}

// 向网站汇报通道的运行状态 => 0(等待) 1(运行) 2(录像)...
bool CWebThread::doWebStatCamera(int nDBCamera, int nStatus)
{
	// 获取网站配置信息...
	string & strWebClass = App()->GetWebClass();
	if ( strWebClass.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kStatCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	char strPost[MAX_PATH] = { 0 };
	char strUrl[MAX_PATH] = { 0 };
	//char szErrMsg[MAX_PATH] = { 0 };
	//string strUTF8Err = ((lpszErrMsg != NULL) ? CUtilTool::ANSI_UTF8(lpszErrMsg) : "");
	//StringParser::EncodeURI(strUTF8Err.c_str(), strUTF8Err.size(), szErrMsg, MAX_PATH);
	//strPost.Format("camera_id=%d&status=%d&err_code=%d&err_msg=%s", nDBCamera, nStatus, nErrCode, szErrMsg);
	sprintf(strPost, "camera_id=%d&status=%d", nDBCamera, nStatus);
	// 组合访问链接地址...
	sprintf(strUrl, "%s/wxapi.php/Gather/saveCamera", strWebClass.c_str());
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if (curl == NULL) break;
		// 如果是https://协议，需要新增参数...
		if ( App()->IsClassHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(strPost));
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要网站返回的数据...
		//res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		//res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while (false);
	// 释放资源...
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}
	return true;
}

bool CWebThread::doWebGatherLogout()
{
	this->LogoutGather();
	this->LogoutHaoYi();
	return true;
}

void CWebThread::Entry()
{
	// 连接过程中不可以重连...
	m_bIsCanReConnect = false;
	m_bIsLoadSuccess = false;
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
		// 网站授权结束，可以进行窗口的创建和显示了 => 通过信号槽代替消息异步发送...
		emit App()->msgFromWebThread(WM_WEB_LOAD_RESOURCE, 0, 0);
		// 设置已经成功连接服务器标志...
		m_bIsLoadSuccess = true;
	} while (false);
	// 如果连接失败，需要退出已经连接的部分内容...
	if (!m_bIsLoadSuccess) {
		this->doWebGatherLogout();
	}
	// 设置可以重连服务器标志...
	m_bIsCanReConnect = true;
}