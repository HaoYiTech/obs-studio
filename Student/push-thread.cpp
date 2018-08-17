
#include "speex-aec.h"
#include "student-app.h"
#include "pull-thread.h"
#include "push-thread.h"
#include "UDPSendThread.h"

#include "window-view-camera.hpp"

CPushThread::CPushThread(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
  , m_lpUDPSendThread(NULL)
  , m_lpRtspThread(NULL)
  , m_lpSpeexAEC(NULL)
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
	blog(LOG_INFO, "== [~CPushThread Thread] - Exit Start ==");
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
	// ɾ��������������...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	blog(LOG_INFO, "== [~CPushThread Thread] - Exit End ==");
}

string CPushThread::GetAVCHeader()
{
	string strAVCHeader;
	if (m_lpRtspThread != NULL) {
		strAVCHeader = m_lpRtspThread->GetAVCHeader();
	}
	return strAVCHeader;
}

string CPushThread::GetAACHeader()
{
	string strAACHeader;
	if (m_lpRtspThread != NULL) {
		strAACHeader = m_lpRtspThread->GetAACHeader();
	}
	return strAACHeader;
}

string CPushThread::GetVideoSPS()
{
	string strVideoSPS;
	if (m_lpRtspThread != NULL) {
		strVideoSPS = m_lpRtspThread->GetVideoSPS();
	}
	return strVideoSPS;
}

string CPushThread::GetVideoPPS()
{
	string strVideoPPS;
	if (m_lpRtspThread != NULL) {
		strVideoPPS = m_lpRtspThread->GetVideoPPS();
	}
	return strVideoPPS;
}

int CPushThread::GetVideoFPS()
{
	return ((m_lpRtspThread != NULL) ? m_lpRtspThread->GetVideoFPS() : 0);
}

int CPushThread::GetVideoWidth()
{
	return ((m_lpRtspThread != NULL) ? m_lpRtspThread->GetVideoWidth() : 0);
}

int CPushThread::GetVideoHeight()
{
	return ((m_lpRtspThread != NULL) ? m_lpRtspThread->GetVideoHeight() : 0);
}

int CPushThread::GetAudioRateIndex()
{
	return ((m_lpRtspThread != NULL) ? m_lpRtspThread->GetAudioRateIndex() : 0);
}

int CPushThread::GetAudioChannelNum()
{
	return ((m_lpRtspThread != NULL) ? m_lpRtspThread->GetAudioChannelNum() : 0);
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

// rtsp�����߳��Ѿ�׼������...
void CPushThread::onRtspReady()
{
	blog(LOG_INFO, "CPushThread::onRtspReady() => ID: %d", m_nDBCameraID);
	// ֪ͨ����ͷ������������㱨״̬...
	if (m_lpViewCamera != NULL) {
		m_lpViewCamera->doStatReport();
	}
	// �ؽ�����ʼ����Ƶ������������...
	if (m_lpSpeexAEC != NULL) {
		delete m_lpSpeexAEC;
		m_lpSpeexAEC = NULL;
	}
	// ��ȡ��Ƶ��صĸ�ʽͷ��Ϣ...
	int nRateIndex = this->GetAudioRateIndex();
	int nChannelNum = this->GetAudioChannelNum();
	// ��������ʼ��������������...
	m_lpSpeexAEC = new CSpeexAEC(this);
	m_lpSpeexAEC->InitSpeex(nRateIndex, nChannelNum);
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
	//OSMutexLocker theLock(&m_Mutex);
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
		m_lpUDPSendThread = new CUDPSendThread(this, nRoomID, nDBCameraID);
		// �����ʼ��UDP�����߳�ʧ�ܣ�ɾ���Ѿ������Ķ���...
		if( !m_lpUDPSendThread->InitThread(strUdpAddr, nUdpPort) ) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
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
	//OSMutexLocker theLock(&m_Mutex);
	if (m_lpRtspThread == NULL)
		return;
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	// ���UDP�����߳���Ч�����ң������߳�û�з�������ӵ����������������...
	if (m_lpUDPSendThread != NULL && !m_lpUDPSendThread->IsNeedDelete()) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// �������Ƶ����֡����Ҫ���л�������...
	if (m_lpSpeexAEC != NULL && inFrame.typeFlvTag == PT_TAG_AUDIO) {
		m_lpSpeexAEC->PushFrame(inFrame);
	}
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
	//OSMutexLocker theLock(&m_Mutex);
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
