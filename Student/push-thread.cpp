
#include "student-app.h"
#include "pull-thread.h"
#include "push-thread.h"
#include "UDPSendThread.h"

#include "window-view-camera.hpp"

CPushThread::CPushThread(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
  , m_lpUDPSendThread(NULL)
  , m_lpRtspThread(NULL)
  , m_nCurRecvByte(0)
  , m_dwTimeOutMS(0)
  , m_nFlowTimer(-1)
  , m_nRecvKbps(0)
{
	ASSERT(m_lpViewCamera != NULL);
	m_nDBCameraID = m_lpViewCamera->GetDBCameraID();
}

CPushThread::~CPushThread()
{
	// 注意：这里使用互斥会造成卡死...
	// 这里不能使用互斥，删除RTSP拉流对象...
	if (m_lpRtspThread != NULL) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
	}
	// 删除UDP推流线程 => 数据使用者...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	blog(LOG_INFO, "== [~CPushThread Thread] - Exit ==");
}

bool CPushThread::InitThread()
{
	// 获取当前摄像头存放的配置...
	GM_MapData theMapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	ASSERT(theMapData.size() > 0);
	// 获取需要的流转发参数信息...
	string & strUsingTCP = theMapData["use_tcp"];
	string & strStreamMP4 = theMapData["stream_mp4"];
	string & strStreamUrl = theMapData["stream_url"];
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	bool bFileMode = ((nStreamProp == kStreamUrlLink) ? false : true);
	bool bUsingTCP = ((strUsingTCP.size() > 0) ? atoi(strUsingTCP.c_str()) : false);
	// 目前暂时只能处理rtsp数据流格式...
	if (bFileMode || strnicmp("rtsp://", strStreamUrl.c_str(), strlen("rtsp://")) != 0)
		return false;
	// 创建新的RTSP拉流对象...
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, bUsingTCP, strStreamUrl);
	// 记录当前时间，单位（毫秒）...
	DWORD dwInitTimeMS = ::GetTickCount();
	// 初始化接收数据超时时间记录起点 => 超过 1 分钟无数据接收，则判定为超时...
	m_dwTimeOutMS = dwInitTimeMS;
	// 开启一个定时检测时钟 => 每隔1秒执行一次...
	m_nFlowTimer = this->startTimer(1 * 1000);
	return true;
}

void CPushThread::timerEvent(QTimerEvent * inEvent)
{
	int nTimerID = inEvent->timerId();
	if (nTimerID == m_nFlowTimer) {
		this->CalcFlowKbps();
	}
}

void CPushThread::CalcFlowKbps()
{
	// 计算实际的接收码率 => Kbps...
	m_nRecvKbps = (m_nCurRecvByte * 8) / 1024;
	m_nCurRecvByte = 0;
	// 通知摄像头窗口更新显示层...
	m_lpViewCamera->update();
}

void CPushThread::onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID)
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果是通道开始推流命令 => 创建推流线程...
	if (bIsStartCmd) {
		// 如果推流线程已经创建了，直接返回...
		if (m_lpUDPSendThread != NULL)
			return;
		ASSERT(m_lpUDPSendThread == NULL);
		// 创建UDP推流线程，使用服务器传递过来的参数...
		int nRoomID = atoi(App()->GetRoomIDStr().c_str());
		string & strUdpAddr = App()->GetUdpAddr();
		int nUdpPort = App()->GetUdpPort();
		m_lpUDPSendThread = new CUDPSendThread(m_lpViewCamera, nRoomID, nDBCameraID);
		m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort);
	} else {
		// 如果是通道停止推流命令 => 删除推流线程...
		if (m_lpUDPSendThread != NULL) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	}
}

void CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	// 这里使用了互斥，拉流对象已经被删除，直接返回...
	OSMutexLocker theLock(&m_Mutex);
	if (m_lpRtspThread == NULL)
		return;
	// 将超时计时点复位，重新计时...
	m_dwTimeOutMS = ::GetTickCount();
	// 累加接收数据包的字节数，加入缓存队列...
	m_nCurRecvByte += inFrame.strData.size();
	// 如果UDP推流线程有效，继续传递数据...
	if (m_lpUDPSendThread != NULL) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// 可以进一步的对数据帧进行加工处理...
}

QString CPushThread::GetStreamPushUrl()
{
	if (m_lpUDPSendThread == NULL) {
		return QTStr("Camera.Window.None");
	}
	string & strServerAddr = m_lpUDPSendThread->GetServerAddrStr();
	QString strQServerStr = QString::fromUtf8(strServerAddr.c_str());
	int nServerPort = m_lpUDPSendThread->GetServerPortInt();
	return QString("udp://%1/%2").arg(strQServerStr).arg(nServerPort);
}

int CPushThread::GetRecvPullKbps()
{
	// 如果发生超时，返回 -1，等待删除...
	if (this->IsFrameTimeout()) {
		MsgLogGM(GM_Err_Timeout);
		return -1;
	}
	// 返回接收码流...
	return m_nRecvKbps;
}

int CPushThread::GetSendPushKbps()
{
	return ((m_lpUDPSendThread != NULL) ? m_lpUDPSendThread->GetSendTotalKbps() : -1);
}

bool CPushThread::IsDataFinished()
{
	// 这里使用了互斥，避免拉流对象无效...
	OSMutexLocker theLock(&m_Mutex);
	if (m_lpRtspThread != NULL) {
		return m_lpRtspThread->IsFinished();
	}
	return true;
}

bool CPushThread::IsFrameTimeout()
{
	// 首先判断数据线程是否已经结束了...
	if (this->IsDataFinished())
		return true;
	// 一直没有新数据到达，超过 1 分钟，则判定为超时...
	int nWaitMinute = 1;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
