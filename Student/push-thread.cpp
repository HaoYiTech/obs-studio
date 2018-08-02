
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
	// ע�⣺����ʹ�û������ɿ���...
	// ���ﲻ��ʹ�û��⣬ɾ��RTSP��������...
	if (m_lpRtspThread != NULL) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
	}
	// ɾ��UDP�����߳� => ����ʹ����...
	if (m_lpUDPSendThread != NULL) {
		delete m_lpUDPSendThread;
		m_lpUDPSendThread = NULL;
	}
	blog(LOG_INFO, "== [~CPushThread Thread] - Exit ==");
}

bool CPushThread::InitThread()
{
	// ��ȡ��ǰ����ͷ��ŵ�����...
	GM_MapData theMapData;
	App()->GetCamera(m_nDBCameraID, theMapData);
	ASSERT(theMapData.size() > 0);
	// ��ȡ��Ҫ����ת��������Ϣ...
	string & strUsingTCP = theMapData["use_tcp"];
	string & strStreamMP4 = theMapData["stream_mp4"];
	string & strStreamUrl = theMapData["stream_url"];
	string & strStreamProp = theMapData["stream_prop"];
	int  nStreamProp = atoi(strStreamProp.c_str());
	bool bFileMode = ((nStreamProp == kStreamUrlLink) ? false : true);
	bool bUsingTCP = ((strUsingTCP.size() > 0) ? atoi(strUsingTCP.c_str()) : false);
	// Ŀǰ��ʱֻ�ܴ���rtsp��������ʽ...
	if (bFileMode || strnicmp("rtsp://", strStreamUrl.c_str(), strlen("rtsp://")) != 0)
		return false;
	// �����µ�RTSP��������...
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, bUsingTCP, strStreamUrl);
	// ��¼��ǰʱ�䣬��λ�����룩...
	DWORD dwInitTimeMS = ::GetTickCount();
	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 1 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = dwInitTimeMS;
	// ����һ����ʱ���ʱ�� => ÿ��1��ִ��һ��...
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
	// ����ʵ�ʵĽ������� => Kbps...
	m_nRecvKbps = (m_nCurRecvByte * 8) / 1024;
	m_nCurRecvByte = 0;
	// ֪ͨ����ͷ���ڸ�����ʾ��...
	m_lpViewCamera->update();
}

void CPushThread::onTriggerUdpSendThread(bool bIsStartCmd, int nDBCameraID)
{
	OSMutexLocker theLock(&m_Mutex);
	// �����ͨ����ʼ�������� => ���������߳�...
	if (bIsStartCmd) {
		// ��������߳��Ѿ������ˣ�ֱ�ӷ���...
		if (m_lpUDPSendThread != NULL)
			return;
		ASSERT(m_lpUDPSendThread == NULL);
		// ����UDP�����̣߳�ʹ�÷��������ݹ����Ĳ���...
		int nRoomID = atoi(App()->GetRoomIDStr().c_str());
		string & strUdpAddr = App()->GetUdpAddr();
		int nUdpPort = App()->GetUdpPort();
		m_lpUDPSendThread = new CUDPSendThread(m_lpViewCamera, nRoomID, nDBCameraID);
		m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort);
	} else {
		// �����ͨ��ֹͣ�������� => ɾ�������߳�...
		if (m_lpUDPSendThread != NULL) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	}
}

void CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	// ����ʹ���˻��⣬���������Ѿ���ɾ����ֱ�ӷ���...
	OSMutexLocker theLock(&m_Mutex);
	if (m_lpRtspThread == NULL)
		return;
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	// ���UDP�����߳���Ч��������������...
	if (m_lpUDPSendThread != NULL) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// ���Խ�һ���Ķ�����֡���мӹ�����...
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
	// ���������ʱ������ -1���ȴ�ɾ��...
	if (this->IsFrameTimeout()) {
		MsgLogGM(GM_Err_Timeout);
		return -1;
	}
	// ���ؽ�������...
	return m_nRecvKbps;
}

int CPushThread::GetSendPushKbps()
{
	return ((m_lpUDPSendThread != NULL) ? m_lpUDPSendThread->GetSendTotalKbps() : -1);
}

bool CPushThread::IsDataFinished()
{
	// ����ʹ���˻��⣬��������������Ч...
	OSMutexLocker theLock(&m_Mutex);
	if (m_lpRtspThread != NULL) {
		return m_lpRtspThread->IsFinished();
	}
	return true;
}

bool CPushThread::IsFrameTimeout()
{
	// �����ж������߳��Ƿ��Ѿ�������...
	if (this->IsDataFinished())
		return true;
	// һֱû�������ݵ������ 1 ���ӣ����ж�Ϊ��ʱ...
	int nWaitMinute = 1;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
