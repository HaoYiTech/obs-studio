
#include "student-app.h"
#include "pull-thread.h"
#include "push-thread.h"

#include "window-view-camera.hpp"

CPushThread::CPushThread(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
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
	if (m_lpRtspThread != NULL) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
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

void CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
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
	return 0;
}

bool CPushThread::IsDataFinished()
{
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
