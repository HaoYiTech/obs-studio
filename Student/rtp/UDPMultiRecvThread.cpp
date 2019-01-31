
#include "UDPSocket.h"
#include "student-app.h"
#include "UDPPlayThread.h"
#include "UDPMultiRecvThread.h"
#include "../window-view-render.hpp"

CUDPMultiRecvThread::CUDPMultiRecvThread(CViewRender * lpViewRender)
  : m_lpViewRender(lpViewRender)
  , m_bNeedSleep(false)
  , m_lpPlaySDL(NULL)
  , m_lpUdpData(NULL)
  , m_lpUdpLose(NULL)
  , m_nMaxResendCount(0)
  , m_nAudioMaxPlaySeq(0)
  , m_nVideoMaxPlaySeq(0)
  , m_bFirstAudioSeq(false)
  , m_bFirstVideoSeq(false)
  , m_server_cache_time_ms(-1)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
  , m_sys_zero_ns(-1)
{
	ASSERT(m_lpViewRender != NULL);
	// ��ʼ������״̬...
	m_nCmdState = kCmdSendCreate;
	// ��ʼ��rtp����ͷ�ṹ��...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_supply, 0, sizeof(m_rtp_supply));
	// ��ʼ�����������ʽ��Ϣ...
	m_rtp_supply.tm = TM_TAG_STUDENT;
	m_rtp_supply.id = ID_TAG_LOOKER;
	m_rtp_supply.pt = PT_TAG_SUPPLY;
	// ��ʼ������Ƶ���ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	// ��ʼ����ʱ0��ʱ�̵�...
	m_time_zero_ns = os_gettime_ns();
}

CUDPMultiRecvThread::~CUDPMultiRecvThread()
{
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// �ر�UDPSocket����...
	this->ClearAllSocket();
	// ɾ������Ƶ�����߳�...
	this->ClosePlayer();
	// �ͷ�����Ƶ���ζ��пռ�...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
	// ֪ͨ��ര�ڣ������߳��Ѿ�ֹͣ...
	App()->onUdpRecvThreadStop();

	blog(LOG_INFO, "%s == [~CUDPMultiRecvThread Exit] ==", TM_RECV_NAME);
}

void CUDPMultiRecvThread::ClosePlayer()
{
	if (m_lpPlaySDL != NULL) {
		delete m_lpPlaySDL;
		m_lpPlaySDL = NULL;
	}
}

void CUDPMultiRecvThread::ClearAllSocket()
{
	if (m_lpUdpData != NULL) {
		delete m_lpUdpData;
		m_lpUdpData = NULL;
	}
	if (m_lpUdpLose != NULL) {
		delete m_lpUdpLose;
		m_lpUdpLose = NULL;
	}
}

BOOL CUDPMultiRecvThread::InitThread()
{
	// ��ɾ���Ѿ����ڵ��鲥�׽��ֶ���...
	this->ClearAllSocket();
	// �ؽ��鲥���ݽ��ն���...
	if (!this->BuildDataSocket())
		return false;
	// �ؽ��鲥�������Ͷ���...
	if (!this->BuildLoseSocket())
		return false;
	// �����鲥�����߳�...
	this->Start();
	// ����ִ�н��...
	return true;
}

BOOL CUDPMultiRecvThread::BuildDataSocket()
{
	// ��ȡ�鲥��ַ���鲥�˿�(�����)�ȵ�...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwDataMultiAddr = inet_addr(DEF_MULTI_DATA_ADDR);
	// �ؽ��鲥�׽��ֶ���...
	ASSERT(m_lpUdpData == NULL);
	m_lpUdpData = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺�����UDP�鲥�׽���ʹ���첽ģʽ...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpData->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����첽ģʽ...
	m_lpUdpData->NonBlocking();
	// �����ظ�ʹ�ö˿�...
	m_lpUdpData->ReuseAddr();
	// ������ => ���鲥�˿ں��룬��ʧ�ܣ����ش���...
	theErr = m_lpUdpData->Bind(INADDR_ANY, nMultiPort);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// ���÷��ͺͽ��ջ�����...
	m_lpUdpData->SetSocketSendBufSize(128 * 1024);
	m_lpUdpData->SetSocketRecvBufSize(128 * 1024);
	// ����TTL���紩Խ��ֵ...
	m_lpUdpData->SetMulticastTTL(32);
	m_lpUdpData->SetMulticastLoop(false);
	// ����ָ�����鲥���ַ => ���ͽӿ�ʹ��Ĭ�Ͻӿ�...
	theErr = m_lpUdpData->JoinMulticastMember(dwDataMultiAddr, INADDR_ANY);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����鲥���͵�Զ�̵�ַ���� SendTo() ���п��Լ򻯽ӿ�...
	m_lpUdpData->SetRemoteAddr(DEF_MULTI_DATA_ADDR, nMultiPort);
	return true;
}

BOOL CUDPMultiRecvThread::BuildLoseSocket()
{
	// ��ȡ�鲥��ַ���鲥�˿�(�����)�ȵ�...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwLoseMultiAddr = inet_addr(DEF_MULTI_LOSE_ADDR);
	// �ؽ��鲥�׽��ֶ���...
	ASSERT(m_lpUdpLose == NULL);
	m_lpUdpLose = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺�����UDP�鲥�׽���ʹ���첽ģʽ...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpLose->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����첽ģʽ...
	m_lpUdpData->NonBlocking();
	// �����ظ�ʹ�ö˿�...
	m_lpUdpLose->ReuseAddr();
	// ���÷��ͺͽ��ջ�����...
	m_lpUdpLose->SetSocketSendBufSize(128 * 1024);
	m_lpUdpLose->SetSocketRecvBufSize(128 * 1024);
	// ����TTL���紩Խ��ֵ...
	m_lpUdpLose->SetMulticastTTL(32);
	m_lpUdpLose->SetMulticastLoop(false);
	// ����ָ�����鲥���ַ => ���ͽӿ�ʹ��Ĭ�Ͻӿ�...
	theErr = m_lpUdpLose->JoinMulticastMember(dwLoseMultiAddr, INADDR_ANY);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����鲥���͵�Զ�̵�ַ���� SendTo() ���п��Լ򻯽ӿ�...
	m_lpUdpLose->SetRemoteAddr(DEF_MULTI_LOSE_ADDR, nMultiPort);
	// ���ö������ͽӿ� => ������ʱ��Ҫָ���鲥��������...
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nInterAddr = inet_addr(strIPSend.c_str());
	// ����鲥���ͽӿڲ���INADDR_ANY���Ž��������޸�...
	if (nInterAddr != INADDR_ANY) {
		theErr = m_lpUdpLose->ModMulticastInterface(nInterAddr);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}
	return true;
}

// �����鲥�����׽��ֵķ��ͽӿ�...
void CUDPMultiRecvThread::doResetMulticastIPSend()
{
	if (m_lpUdpLose == NULL)
		return;
	GM_Error theErr = GM_NoErr;
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nMultiInterAddr = inet_addr(strIPSend.c_str());
	theErr = m_lpUdpLose->ModMulticastInterface(nMultiInterAddr);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
}

void CUDPMultiRecvThread::Entry()
{
	blog(LOG_INFO, "== CUDPMultiRecvThread::Entry() ==");
	// ������ʾ���ڵ�������Ϣ���� => �ȴ��鲥���ݵ���...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WatiMulticast"));
	// ִ���̺߳�����ֱ���߳��˳�...
	while (!this->IsStopRequested()) {
		// ������Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
		m_bNeedSleep = true;
		// ����鲥�����Ƿ�����ʱ...
		this->doCheckRecvTimeout();
		// ����һ������ķ�����������...
		this->doRecvPacket();
		// �ȷ�����Ƶ��������...
		this->doSendSupplyCmd(true);
		// �ٷ�����Ƶ��������...
		this->doSendSupplyCmd(false);
		// �ӻ��ζ����г�ȡ����һ֡��Ƶ���벥����...
		this->doParseFrame(true);
		// �ӻ��ζ����г�ȡ����һ֡��Ƶ���벥����...
		this->doParseFrame(false);
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
}

void CUDPMultiRecvThread::doCheckRecvTimeout()
{
	// ����鲥����û�з�����ʱ��ֱ�ӷ��أ������ȴ��´γ�ʱ���...
	uint32_t nDeltaTime = (uint32_t)((os_gettime_ns() - m_time_zero_ns)/1000000);
	if (nDeltaTime < MAX_MULTI_JAM_MS)
		return;
	// ���ó�ʱʱ��0����ʱ���...
	m_time_zero_ns = os_gettime_ns();
	ASSERT(nDeltaTime >= MAX_MULTI_JAM_MS);
	// �����鲥���ճ�ʱ����Ҫ������ر���...
	m_nCmdState = kCmdSendCreate;
	m_nMaxResendCount = 0;
	m_nAudioMaxPlaySeq = 0;
	m_nVideoMaxPlaySeq = 0;
	m_bFirstAudioSeq = false;
	m_bFirstVideoSeq = false;
	m_server_cache_time_ms = -1;
	m_server_rtt_var_ms = -1;
	m_server_rtt_ms = -1;
	m_sys_zero_ns = -1;
	// ��������ͷ��ʽ��Ϣ...
	m_strSeqHeader.clear();
	m_strSPS.clear();
	m_strPPS.clear();
	// ��ʼ��rtp����ͷ�ṹ��...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	// ��ʼ������Ƶ���ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	// ��ղ�����Ŷ���...
	m_AudioMapLose.clear();
	m_VideoMapLose.clear();
	// ɾ������Ƶ�����߳�...
	this->ClosePlayer();
	// ֪ͨ��ര�ڣ������߳��Ѿ�ֹͣ...
	App()->onUdpRecvThreadStop();
	// ������ʾ���ڵ�������Ϣ���� => �ȴ��鲥���ݵ���...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WatiMulticast"));
	// ��ӡ��ʱ��Ϣ...
	//blog(LOG_INFO, "%s Multicast recv wait time out...", TM_RECV_NAME);
}

void CUDPMultiRecvThread::doRecvPacket()
{
	if (m_lpUdpData == NULL)
		return;
	GM_Error theErr = GM_NoErr;
	UInt32   outRecvLen = 0;
	UInt32   outRemoteAddr = 0;
	UInt16   outRemotePort = 0;
	UInt32   inBufLen = MAX_BUFF_LEN;
	char     ioBuffer[MAX_BUFF_LEN] = { 0 };
	// ���ýӿڴ�������ȡ���ݰ� => �������첽�׽��֣����������� => ���ܴ���...
	theErr = m_lpUdpData->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// ע�⣺���ﲻ�ô�ӡ������Ϣ��û���յ����ݾ���������...
	if (theErr != GM_NoErr || outRecvLen <= 0)
		return;
	// �޸���Ϣ״̬ => �Ѿ��ɹ��հ���������Ϣ...
	m_bNeedSleep = false;
	// �ж����������ݳ��� => DEF_MTU_SIZE + rtp_hdr_t
	UInt32 nMaxSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (outRecvLen > nMaxSize) {
		blog(LOG_INFO, "[Recv-Error] Max => %lu, Addr => %lu:%d, Size => %lu", nMaxSize, outRemoteAddr, outRemotePort, outRecvLen);
		return;
	}
	// ���ó�ʱ0��ʱ�̵�...
	m_time_zero_ns = os_gettime_ns();
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
	uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	// ���յ�������������ͷַ�...
	switch (ptTag)
	{
	case PT_TAG_HEADER:	 this->doProcServerHeader(ioBuffer, outRecvLen); break;
	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_AUDIO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_VIDEO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	}
}

void CUDPMultiRecvThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// ͨ�� rtp_header_t ��Ϊ���巢�͹����� => ������ֱ��ԭ��ת����ѧ�������˵�����ͷ�ṹ��...
	if (m_lpUdpData == NULL || lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_header_t))
		return;
	// ����Ѿ��յ�������ͷ��ֱ�ӷ���...
	if (m_nCmdState != kCmdSendCreate || m_rtp_header.pt == PT_TAG_HEADER || m_rtp_header.hasAudio || m_rtp_header.hasVideo)
		return;
	// ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
	uint8_t tmTag = lpBuffer[0] & 0x03;
	// ��ȡ��һ���ֽڵ���2λ���õ��ն����...
	uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
	uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ��������߲��� ��ʦ������ => ֱ�ӷ���...
	if (tmTag != TM_TAG_TEACHER || idTag != ID_TAG_PUSHER)
		return;
	// ������ֳ��Ȳ�����ֱ�ӷ���...
	memcpy(&m_rtp_header, lpBuffer, sizeof(m_rtp_header));
	int nNeedSize = m_rtp_header.spsSize + m_rtp_header.ppsSize + sizeof(m_rtp_header);
	if (nNeedSize != inRecvLen) {
		blog(LOG_INFO, "%s Recv Header error, RecvLen: %d", TM_RECV_NAME, inRecvLen);
		memset(&m_rtp_header, 0, sizeof(m_rtp_header));
		return;
	}
	// ����ϵͳ0��ʱ��...
	m_sys_zero_ns = os_gettime_ns();
	blog(LOG_INFO, "%s Set System Zero Time By Create => %I64d ms", TM_RECV_NAME, m_sys_zero_ns / 1000000);
	// ������Ⱦ���棬��ʾ�������ӷ���������ʾ��Ϣ...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.ConnectServer"));
	// ֱ�ӽ��յ�������ͷ���������������鲥ת��ʹ��...
	m_strSeqHeader.assign(lpBuffer, inRecvLen);
	// ��ȡSPS��PPS��ʽͷ��Ϣ...
	char * lpData = lpBuffer + sizeof(m_rtp_header);
	if (m_rtp_header.spsSize > 0) {
		m_strSPS.assign(lpData, m_rtp_header.spsSize);
		lpData += m_rtp_header.spsSize;
	}
	// ���� PPS ��ʽͷ...
	if (m_rtp_header.ppsSize > 0) {
		m_strPPS.assign(lpData, m_rtp_header.ppsSize);
		lpData += m_rtp_header.ppsSize;
	}
	// �޸�����״̬ => ��ʼ�������׼�����������...
	m_nCmdState = kCmdConnetOK;
	// ��ӡ�յ�����ͷ�ṹ����Ϣ...
	blog(LOG_INFO, "%s Recv Header SPS: %d, PPS: %d", TM_RECV_NAME, m_strSPS.size(), m_strPPS.size());
	// ����������Ѿ�������ֱ�ӷ���...
	if (m_lpPlaySDL != NULL || m_lpViewRender == NULL)
		return;
	// ������Ⱦ���棬�����ӳɹ����ȴ�������������...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WaitNetData"));
	// �½�����������ʼ������Ƶ�߳�...
	m_lpPlaySDL = new CPlaySDL(m_lpViewRender, m_sys_zero_ns);
	// ����Ĭ�ϵ����绺������ʱ�� => ʹ��֡��������...
	m_server_cache_time_ms = 1000 / ((m_rtp_header.fpsNum > 0) ? m_rtp_header.fpsNum : 25);
	// �������Ƶ����ʼ����Ƶ�߳�...
	if (m_rtp_header.hasVideo) {
		int nPicFPS = m_rtp_header.fpsNum;
		int nPicWidth = m_rtp_header.picWidth;
		int nPicHeight = m_rtp_header.picHeight;
		m_lpPlaySDL->InitVideo(m_strSPS, m_strPPS, nPicWidth, nPicHeight, nPicFPS);
	}
	// �������Ƶ����ʼ����Ƶ�߳�...
	if (m_rtp_header.hasAudio) {
		int nInRateIndex = m_rtp_header.rateIndex;
		int nInChannelNum = m_rtp_header.channelNum;
		m_lpPlaySDL->InitAudio(nInRateIndex, nInChannelNum);
	}
}

void CUDPMultiRecvThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	// ���û���յ�����ͷ�����ܽ������ݴ���...
	if (m_nCmdState != kCmdConnetOK)
		return;
	ASSERT(m_nCmdState >= kCmdConnetOK);
	// �����������ݸ�ʽ��Ч��ֱ�ӷ���...
	if (lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t))
		return;
	// ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
	uint8_t tmTag = lpBuffer[0] & 0x03;
	// ��ȡ��һ���ֽڵ���2λ���õ��ն����...
	uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
	uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ѧ���ۿ��ˣ�ֻ���������Լ���̽���������������ʱ...
	if (tmTag == TM_TAG_STUDENT && idTag == ID_TAG_LOOKER) {
		// ��ȡ�յ���̽�����ݰ�...
		rtp_detect_t rtpDetect = { 0 };
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// Ŀǰֻ�������Է����������̽����...
		if (rtpDetect.dtDir != DT_TO_SERVER)
			return;
		ASSERT(rtpDetect.dtDir == DT_TO_SERVER);
		// �ȴ���������ش�����Ƶ��С��Ű�...
		this->doServerMinSeq(true, rtpDetect.maxAConSeq);
		// �ٴ���������ش�����Ƶ��С��Ű�...
		this->doServerMinSeq(false, rtpDetect.maxVConSeq);
		
		// ע�⣺������ʱ���鲥���Ͷ��Ѿ�������ϵ���ֵ...
		int keep_rtt = rtpDetect.tsSrc;
		// �����Ƶ����Ƶ��û�ж�����ֱ���趨����ط�����Ϊ0...
		if (m_AudioMapLose.size() <= 0 && m_VideoMapLose.size() <= 0) {
			m_nMaxResendCount = 0;
		}
		// ��ֹ����ͻ���ӳ�����, ��� TCP �� RTT ����˥�����㷨...
		if( m_server_rtt_ms < 0 ) { m_server_rtt_ms = keep_rtt; }
		else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
		// �������綶����ʱ���ֵ => RTT������ֵ...
		if( m_server_rtt_var_ms < 0 ) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
		else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
		// ���㻺������ʱ�� => ���û�ж�����ʹ������������ʱ+���綶����ʱ֮��...
		if( m_nMaxResendCount > 0 ) {
			m_server_cache_time_ms = (2 * m_nMaxResendCount + 1) * (m_server_rtt_ms + m_server_rtt_var_ms) / 2;
		} else {
			m_server_cache_time_ms = m_server_rtt_ms + m_server_rtt_var_ms;
		}

		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		blog(LOG_INFO, "%s Recv Detect => Dir: %d, dtNum: %d, rtt: %d ms, rtt_var: %d ms, cache_time: %d ms, ACircle: %d, VCircle: %d", TM_RECV_NAME,
			 rtpDetect.dtDir, rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms, m_server_cache_time_ms,
			 m_audio_circle.size/nPerPackSize, m_video_circle.size/nPerPackSize);
		// ��ӡ�������ײ�Ļ���״̬��Ϣ...
		/*if (m_lpPlaySDL != NULL) {
			blog(LOG_INFO, "%s Recv Detect => APacket: %d, VPacket: %d, AFrame: %d, VFrame: %d", TM_RECV_NAME,
				m_lpPlaySDL->GetAPacketSize(), m_lpPlaySDL->GetVPacketSize(), m_lpPlaySDL->GetAFrameSize(), m_lpPlaySDL->GetVFrameSize());
		}*/
	}
}

void CUDPMultiRecvThread::doServerMinSeq(bool bIsAudio, uint32_t inMinSeq)
{
	// �������ݰ����ͣ��ҵ��������ϡ����ζ��С���󲥷Ű�...
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	uint32_t  & nMaxPlaySeq = bIsAudio ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	// ����������С������Ч��ֱ�ӷ���...
	if (inMinSeq <= 0)
		return;
	// ����������Ч���Ž��ж�����ѯ����...
	if (theMapLose.size() > 0) {
		// �����������У�����С�ڷ���������С��ŵĶ�������Ҫ�ӵ�...
		GM_MapLose::iterator itorItem = theMapLose.begin();
		while (itorItem != theMapLose.end()) {
			rtp_lose_t & rtpLose = itorItem->second;
			// ���Ҫ���İ��ţ���С����С���ţ����Բ���...
			if (rtpLose.lose_seq >= inMinSeq) {
				itorItem++;
				continue;
			}
			// ���Ҫ���İ��ţ�����С���Ż�ҪС��ֱ�Ӷ������Ѿ�������...
			theMapLose.erase(itorItem++);
		}
	}
	// ������ζ���Ϊ�գ���������ֱ�ӷ���...
	if (cur_circle.size <= 0)
		return;
	// ��ȡ���ζ��е�����С��ź������ţ��ҵ�����߽�...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_seq = 0, max_seq = 0;
	// ��ȡ��С�����ݰ������ݣ���ȡ��С���к�...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpCurHeader->seq;
	// ��ȡ�������ݰ������ݣ���ȡ������к�...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	// ������ζ����е���С��Ű��ȷ������˵���С��Ű��󣬲���������...
	if (min_seq >= inMinSeq)
		return;
	// ��ӡ���ζ�������ǰ�ĸ�������״ֵ̬...
	blog(LOG_INFO, "%s Clear => Audio: %d, ServerMin: %lu, MinSeq: %lu, MaxSeq: %lu, MaxPlaySeq: %lu", TM_RECV_NAME, bIsAudio, inMinSeq, min_seq, max_seq, nMaxPlaySeq);
	// ������ζ����е������Ű��ȷ������˵���С��Ű�С������ȫ������...
	if (max_seq < inMinSeq) {
		inMinSeq = max_seq + 1;
	}
	// ��������Χ => [min_seq, inMinSeq]...
	int nConsumeSize = (inMinSeq - min_seq) * nPerPackSize;
	circlebuf_pop_front(&cur_circle, NULL, nConsumeSize);
	// ����󲥷Ű��趨Ϊ => ��С��Ű� - 1...
	nMaxPlaySeq = inMinSeq - 1;
}

void CUDPMultiRecvThread::doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID)
{
	// �������ݰ����ͣ��ҵ���������...
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// ���û���ҵ�ָ�������кţ�ֱ�ӷ���...
	GM_MapLose::iterator itorItem = theMapLose.find(inSeqID);
	if (itorItem == theMapLose.end())
		return;
	// ɾ����⵽�Ķ����ڵ�...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	theMapLose.erase(itorItem);
	// ��ӡ���յ��Ĳ�����Ϣ����ʣ�µ�δ��������...
	//blog(LOG_INFO, "%s Supply Erase => LoseSeq: %lu, ResendCount: %lu, LoseSize: %lu, Type: %d", TM_RECV_NAME, inSeqID, nResendCount, theMapLose.size(), inPType);
}

// ����ʧ���ݰ�Ԥ�����ζ��л���ռ�...
void CUDPMultiRecvThread::doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID)
{
	// �������ݰ����ͣ��ҵ���������...
	circlebuf & cur_circle = (inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// ��Ҫ�����綶��ʱ��������·ѡ�� => ֻ��һ������������·��...
	int cur_rtt_var_ms = m_server_rtt_var_ms;
	// ׼�����ݰ��ṹ�岢���г�ʼ�� => �������������ó���ͬ���ط�ʱ��㣬���򣬻��������ǳ���Ĳ�������...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	uint32_t sup_id = nStartLoseID;
	rtp_hdr_t rtpDis = { 0 };
	rtpDis.pt = PT_TAG_LOSE;
	// ע�⣺�Ǳ����� => [nStartLoseID, nEndLoseID]
	while (sup_id <= nEndLoseID) {
		// ����ǰ����Ԥ��������...
		rtpDis.seq = sup_id;
		circlebuf_push_back(&cur_circle, &rtpDis, sizeof(rtpDis));
		circlebuf_push_back_zero(&cur_circle, DEF_MTU_SIZE);
		// ��������ż��붪�����е��� => ����ʱ�̵�...
		rtp_lose_t rtpLose = { 0 };
		rtpLose.resend_count = 0;
		rtpLose.lose_seq = sup_id;
		rtpLose.lose_type = inPType;
		// ע�⣺��Ҫ�����綶��ʱ��������·ѡ�� => Ŀǰֻ��һ������������·��...
		// ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽��������Ҳ����Ϊ0�������ҷ���...
		// �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ��� => ���ⲻ�Ƕ�����ֻ�����������...
		rtpLose.resend_time = cur_time_ms + max(cur_rtt_var_ms, MAX_SLEEP_MS);
		theMapLose[sup_id] = rtpLose;
		// ��ӡ�Ѷ�����Ϣ���������г���...
		//blog(LOG_INFO, "%s Lose Seq: %lu, LoseSize: %lu, Type: %d", TM_RECV_NAME, sup_id, theMapLose.size(), inPType);
		// �ۼӵ�ǰ�������к�...
		++sup_id;
	}
}

void CUDPMultiRecvThread::doTagAVPackProcess(char * lpBuffer, int inRecvLen)
{
	// ���û���յ�����ͷ�����ܽ������ݴ���...
	if (m_nCmdState != kCmdConnetOK)
		return;
	ASSERT(m_nCmdState >= kCmdConnetOK);
	// �ж��������ݰ�����Ч�� => ����С�����ݰ���ͷ�ṹ����...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (lpBuffer == NULL || inRecvLen < sizeof(rtp_hdr_t) || inRecvLen > nPerPackSize) {
		blog(LOG_INFO, "%s Error => RecvLen: %d, Max: %d", TM_RECV_NAME, inRecvLen, nPerPackSize);
		return;
	}
	// ����յ��Ļ��������Ȳ��� �� �����Ϊ������ֱ�Ӷ���...
	rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
	int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
	int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
	uint8_t  pt_tag = lpNewHeader->pt;
	uint32_t new_id = lpNewHeader->seq;
	uint32_t max_id = new_id;
	uint32_t min_id = new_id;
	// ���ִ�����󣬶������������ӡ������Ϣ...
	if (inRecvLen != nDataSize || nZeroSize < 0) {
		blog(LOG_INFO, "%s Error => RecvLen: %d, DataSize: %d, ZeroSize: %d", TM_RECV_NAME, inRecvLen, nDataSize, nZeroSize);
		return;
	}
	// ����Ƶʹ�ò�ͬ�Ĵ������ͱ���...
	uint32_t & nMaxPlaySeq = (pt_tag == PT_TAG_AUDIO) ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	bool   &  bFirstSeqSet = (pt_tag == PT_TAG_AUDIO) ? m_bFirstAudioSeq : m_bFirstVideoSeq;
	circlebuf & cur_circle = (pt_tag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	// ע�⣺�ۿ��˺����ʱ����󲥷Ű���Ų��Ǵ�0��ʼ��...
	// �����󲥷����а���0��˵���ǵ�һ��������Ҫ����Ϊ��󲥷Ű� => ��ǰ���� - 1 => ��󲥷Ű�����ɾ��������ǰ����Ŵ�1��ʼ...
	if (nMaxPlaySeq <= 0 && !bFirstSeqSet) {
		bFirstSeqSet = true;
		nMaxPlaySeq = new_id - 1;
		blog(LOG_INFO, "%s First Packet => Seq: %lu, Key: %d, PTS: %lu, PStart: %d, Type: %d", TM_RECV_NAME, new_id, lpNewHeader->pk, lpNewHeader->ts, lpNewHeader->pst, pt_tag);
	}
	// ����յ��Ĳ�����ȵ�ǰ��󲥷Ű���ҪС => ˵���Ƕ�β������������ֱ���ӵ�...
	// ע�⣺��ʹ���ҲҪ�ӵ�����Ϊ��󲥷���Ű������Ѿ�Ͷ�ݵ��˲��Ų㣬�Ѿ���ɾ����...
	if (new_id <= nMaxPlaySeq) {
		//blog(LOG_INFO, "%s Supply Discard => Seq: %lu, MaxPlaySeq: %lu, Type: %d", TM_RECV_NAME, new_id, nMaxPlaySeq, pt_tag);
		return;
	}
	// ��ӡ�յ�����Ƶ���ݰ���Ϣ => ��������������� => ÿ�����ݰ�����ͳһ��С => rtp_hdr_t + slice + Zero
	//blog(LOG_INFO, "%s Size: %d, Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, ZeroSize: %d", TM_RECV_NAME, inRecvLen, lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->pst, lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
	// ���ȣ�����ǰ�����кŴӶ������е���ɾ��...
	this->doEraseLoseSeq(pt_tag, new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ÿ�����ζ����е����ݰ���С��һ���� => rtp_hdr_t + slice + Zero
	//////////////////////////////////////////////////////////////////////////////////////////////////
	static char szPacketBuffer[nPerPackSize] = { 0 };
	// ������ζ���Ϊ�� => ��Ҫ�Զ�������ǰԤ�в����д���...
	if (cur_circle.size < nPerPackSize) {
		// �µ���Ű�����󲥷Ű�֮���п�϶��˵���ж���...
		// ���������� => [nMaxPlaySeq + 1, new_id - 1]
		if (new_id >(nMaxPlaySeq + 1)) {
			this->doFillLosePack(pt_tag, nMaxPlaySeq + 1, new_id - 1);
		}
		// ��������Ű�ֱ��׷�ӵ����ζ��е�����棬�������󲥷Ű�֮���п�϶���Ѿ���ǰ��Ĳ����в������...
		// �ȼ����ͷ����������...
		circlebuf_push_back(&cur_circle, lpBuffer, inRecvLen);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if (nZeroSize > 0) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// ��ӡ��׷�ӵ���Ű� => ������û�ж�������Ҫ׷���������Ű�...
		//blog(LOG_INFO, "%s Max Seq: %lu, Cricle: Zero", TM_RECV_NAME, new_id);
		return;
	}
	// ���ζ���������Ҫ��һ�����ݰ�...
	ASSERT(cur_circle.size >= nPerPackSize);
	// ��ȡ���ζ�������С���к�...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
	min_id = lpMinHeader->seq;
	// ��ȡ���ζ�����������к�...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacketBuffer;
	max_id = lpMaxHeader->seq;
	// ������������ => max_id + 1 < new_id
	// ���������� => [max_id + 1, new_id - 1];
	if (max_id + 1 < new_id) {
		this->doFillLosePack(pt_tag, max_id + 1, new_id - 1);
	}
	// ����Ƕ�����������Ű������뻷�ζ��У�����...
	if (max_id + 1 <= new_id) {
		// �ȼ����ͷ����������...
		circlebuf_push_back(&cur_circle, lpBuffer, inRecvLen);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if (nZeroSize > 0) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// ��ӡ�¼���������Ű�...
		//blog(LOG_INFO, "%s Max Seq: %lu, Circle: %d", TM_RECV_NAME, new_id, m_circle.size/nPerPackSize-1);
		return;
	}
	// ����Ƕ�����Ĳ���� => max_id > new_id
	if (max_id > new_id) {
		// �����С��Ŵ��ڶ������ => ��ӡ����ֱ�Ӷ�����������...
		if (min_id > new_id) {
			//blog(LOG_INFO, "%s Supply Discard => Seq: %lu, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, min_id, max_id, pt_tag);
			return;
		}
		// ��С��Ų��ܱȶ������С...
		ASSERT(min_id <= new_id);
		// ���㻺��������λ��...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// ����ȡ���������ݸ��µ�ָ��λ��...
		circlebuf_place(&cur_circle, nPosition, lpBuffer, inRecvLen);
		// ��ӡ�������Ϣ...
		//blog(LOG_INFO, "%s Supply Success => Seq: %lu, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, min_id, max_id, pt_tag);
		return;
	}
	// ���������δ֪������ӡ��Ϣ...
	//blog(LOG_INFO, "%s Multicast Unknown => Seq: %lu, Slice: %d, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, lpNewHeader->psize, min_id, max_id, pt_tag);
}

void CUDPMultiRecvThread::doSendSupplyCmd(bool bIsAudio)
{
	if (m_lpUdpLose == NULL)
		return;
	// �������ݰ����ͣ��ҵ���������...
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	// ����������϶���Ϊ�գ�ֱ�ӷ���...
	if (theMapLose.size() <= 0)
		return;
	ASSERT(theMapLose.size() > 0);
	// �������Ĳ���������...
	const int nHeadSize = sizeof(m_rtp_supply);
	const int nPerPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacket[nPerPackSize] = { 0 };
	char * lpData = szPacket + nHeadSize;
	// ��ȡ��ǰʱ��ĺ���ֵ => С�ڻ���ڵ�ǰʱ��Ķ�������Ҫ֪ͨ���Ͷ��ٴη���...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	// ��Ҫ�����������ӳ�ֵ������·ѡ�� => ֻ��һ����������·...
	int cur_rtt_ms = m_server_rtt_ms;
	// ���ò�������Ϊ0 => ���¼�����Ҫ�����ĸ���...
	m_rtp_supply.suType = bIsAudio ? PT_TAG_AUDIO : PT_TAG_VIDEO;
	m_rtp_supply.suSize = 0;
	int nCalcMaxResend = 0;
	// �����������У��ҳ���Ҫ�����Ķ������к�...
	GM_MapLose::iterator itorItem = theMapLose.begin();
	while (itorItem != theMapLose.end()) {
		rtp_lose_t & rtpLose = itorItem->second;
		if (rtpLose.resend_time <= cur_time_ms) {
			// ����������峬���趨�����ֵ������ѭ�� => ��ಹ��200��...
			if ((nHeadSize + m_rtp_supply.suSize) >= nPerPackSize)
				break;
			// �ۼӲ������Ⱥ�ָ�룬�����������к�...
			memcpy(lpData, &rtpLose.lose_seq, sizeof(uint32_t));
			m_rtp_supply.suSize += sizeof(uint32_t);
			lpData += sizeof(uint32_t);
			// �ۼ��ط�����...
			++rtpLose.resend_count;
			// ������󶪰��ط�����...
			nCalcMaxResend = max(nCalcMaxResend, rtpLose.resend_count);
			// ע�⣺ͬʱ���͵Ĳ������´�Ҳͬʱ���ͣ������γɶ��ɢ�еĲ�������...
			// ע�⣺���һ������������ʱ��û���յ����������Ҫ�ٴη���������Ĳ�������...
			// ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽��������Ҳ����Ϊ0�������ҷ���...
			// �����´��ش�ʱ��� => cur_time + rtt => ����ʱ�ĵ�ǰʱ�� + ���������ӳ�ֵ => ��Ҫ������·ѡ��...
			rtpLose.resend_time = cur_time_ms + max(cur_rtt_ms, MAX_SLEEP_MS);
			// ���������������1���´β�����Ҫ̫�죬׷��һ����Ϣ����..
			rtpLose.resend_time += ((rtpLose.resend_count > 1) ? MAX_SLEEP_MS : 0);
			//blog(LOG_INFO, "%s Multicast Lose Seq: %lu, Audio: %d", TM_RECV_NAME, rtpLose.lose_seq, bIsAudio);
		}
		// �ۼӶ������Ӷ���...
		++itorItem;
	}
	// ������������Ϊ�գ�ֱ�ӷ���...
	if (m_rtp_supply.suSize <= 0)
		return;
	// ��������������󶪰��ط�����...
	m_nMaxResendCount = nCalcMaxResend;
	// ���²�������ͷ���ݿ�...
	memcpy(szPacket, &m_rtp_supply, nHeadSize);
	// ����������岻Ϊ�գ��Ž��в��������...
	GM_Error theErr = GM_NoErr;
	int nDataSize = nHeadSize + m_rtp_supply.suSize;
	////////////////////////////////////////////////////////////////////////////////////////
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ� => ���ݷ�����·������ѡ���·�߷���...
	////////////////////////////////////////////////////////////////////////////////////////
	theErr = m_lpUdpLose->SendTo(szPacket, nDataSize);
	// ����д���������ӡ����...
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
	// ��ӡ�ѷ��Ͳ�������...
	blog(LOG_INFO, "%s Multicast Supply Send => Dir: %d, Count: %d, MaxResend: %d", TM_RECV_NAME, DT_TO_SERVER, m_rtp_supply.suSize/sizeof(uint32_t), nCalcMaxResend);
}

void CUDPMultiRecvThread::doParseFrame(bool bIsAudio)
{
	//////////////////////////////////////////////////////////////////////////////////
	// �����¼��û���յ������������򲥷���Ϊ�գ���ֱ�ӷ��أ������ȴ�...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_nCmdState <= kCmdSendCreate || m_lpPlaySDL == NULL) {
		//blog(LOG_INFO, "%s Wait For Player => Audio: %d, Video: %d", TM_RECV_NAME, m_audio_circle.size/nPerPackSize, m_video_circle.size/nPerPackSize );
		return;
	}

	// ����Ƶʹ�ò�ͬ�Ĵ������ͱ���...
	uint32_t & nMaxPlaySeq = bIsAudio ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺���ζ�������Ҫ��һ�����ݰ����ڣ������ڷ�������ʱ���޷����֣�����Ű��ȵ���С�Ű��󵽣��ͻᱻ�ӵ�...
	// ע�⣺���ζ����ڳ�ȡ��������Ƶ����֮֡��Ҳ���ܱ���ɣ����ԣ������� doFillLosePack �жԻ��ζ���Ϊ��ʱ�����⴦��...
	// ������ζ���Ϊ�ջ򲥷���������Ч��ֱ�ӷ���...
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ׼�����������֡��������Ҫ�ı����Ϳռ����...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketCurrent[nPerPackSize] = { 0 };
	static char szPacketFront[nPerPackSize] = { 0 };
	rtp_hdr_t * lpFrontHeader = NULL;
	if (cur_circle.size <= nPerPackSize)
		return;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	circlebuf_peek_front(&cur_circle, szPacketFront, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketFront;
	// �����С��Ű�����Ҫ����Ķ��� => ������Ϣ�ȴ�...
	if (lpFrontHeader->pt == PT_TAG_LOSE)
		return;
	// �����С��Ű���������Ƶ����֡�Ŀ�ʼ�� => ɾ��������ݰ���������...
	if (lpFrontHeader->pst <= 0) {
		// ���µ�ǰ��󲥷����кŲ���������...
		nMaxPlaySeq = lpFrontHeader->seq;
		// ɾ��������ݰ������ز���Ϣ��������...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
		// �޸���Ϣ״̬ => �Ѿ���ȡ���ݰ���������Ϣ...
		m_bNeedSleep = false;
		// ��ӡ��֡ʧ����Ϣ => û���ҵ�����֡�Ŀ�ʼ���...
		blog(LOG_INFO, "%s Error => Frame start code not find, Seq: %lu, Type: %d, Key: %d, PTS: %lu", TM_RECV_NAME,
			lpFrontHeader->seq, lpFrontHeader->pt, lpFrontHeader->pk, lpFrontHeader->ts);
		return;
	}
	ASSERT(lpFrontHeader->pst);
	// ��ʼ��ʽ�ӻ��ζ����г�ȡ����Ƶ����֡...
	int         pt_type = lpFrontHeader->pt;
	bool        is_key = lpFrontHeader->pk;
	uint32_t    ts_ms = lpFrontHeader->ts;
	uint32_t    min_seq = lpFrontHeader->seq;
	uint32_t    cur_seq = lpFrontHeader->seq;
	rtp_hdr_t * lpCurHeader = lpFrontHeader;
	uint32_t    nConsumeSize = nPerPackSize;
	string      strFrame;
	// ��������֡ => �����������pst��ʼ����ped����...
	while (true) {
		// �����ݰ�����Ч�غɱ�������...
		char * lpData = (char*)lpCurHeader + sizeof(rtp_hdr_t);
		strFrame.append(lpData, lpCurHeader->psize);
		// ������ݰ���֡�Ľ����� => ����֡������...
		if (lpCurHeader->ped > 0)
			break;
		// ������ݰ�����֡�Ľ����� => ����Ѱ��...
		ASSERT(lpCurHeader->ped <= 0);
		// �ۼӰ����кţ���ͨ�����к��ҵ���ͷλ��...
		uint32_t nPosition = (++cur_seq - min_seq) * nPerPackSize;
		// �����ͷ��λλ�ó����˻��ζ����ܳ��� => ˵���Ѿ����ﻷ�ζ���ĩβ => ֱ�ӷ��أ���Ϣ�ȴ�...
		if (nPosition >= cur_circle.size)
			return;
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
		// �ҵ�ָ����ͷλ�õ�ͷָ��ṹ��...
		////////////////////////////////////////////////////////////////////////////////////////////////////
		circlebuf_read(&cur_circle, nPosition, szPacketCurrent, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketCurrent;
		// ����µ����ݰ�������Ч����Ƶ���ݰ� => ���صȴ�����...
		if (lpCurHeader->pt == PT_TAG_LOSE)
			return;
		ASSERT(lpCurHeader->pt != PT_TAG_LOSE);
		// ����µ����ݰ�����������Ű� => ���صȴ�...
		if (cur_seq != lpCurHeader->seq)
			return;
		ASSERT(cur_seq == lpCurHeader->seq);
		// ����ַ�����֡��ʼ��� => ����ѽ�������֡ => �������֡����������Ҫ����...
		// ͬʱ����Ҫ������ʱ��ŵ�����֡�����Ϣ�����¿�ʼ��֡...
		if (lpCurHeader->pst > 0) {
			pt_type = lpCurHeader->pt;
			is_key = lpCurHeader->pk;
			ts_ms = lpCurHeader->ts;
			strFrame.clear();
		}
		// �ۼ��ѽ��������ݰ��ܳ���...
		nConsumeSize += nPerPackSize;
	}
	// ���û�н���������֡ => ��ӡ������Ϣ...
	if (strFrame.size() <= 0) {
		blog(LOG_INFO, "%s Error => Frame size is Zero, PlaySeq: %lu, Type: %d, Key: %d", TM_RECV_NAME, nMaxPlaySeq, pt_type, is_key);
		return;
	}
	// ע�⣺���ζ��б���ɺ󣬱����� doFillLosePack �жԻ��ζ���Ϊ��ʱ�����⴦��...
	// ������ζ��б�ȫ����� => Ҳû��ϵ�����յ��°����жԻ��ζ���Ϊ��ʱ�������⴦��...
	/*if( nConsumeSize >= m_circle.size ) {
		blog(LOG_INFO, "%s Error => Circle Empty, PlaySeq: %lu, CurSeq: %lu", TM_RECV_NAME, m_nMaxPlaySeq, cur_seq);
	}*/

	// ע�⣺�ѽ��������к����Ѿ���ɾ�������к�...
	// ��ǰ�ѽ��������кű���Ϊ��ǰ��󲥷����к�...
	nMaxPlaySeq = cur_seq;
	// ɾ���ѽ�����ϵĻ��ζ������ݰ� => ���ջ�����...
	circlebuf_pop_front(&cur_circle, NULL, nConsumeSize);
	// ��Ҫ�����绺��������ʱʱ�������·ѡ�� => Ŀǰֻ��һ��������·��...
	int cur_cache_ms = m_server_cache_time_ms;
	// ������������Ч����֡���벥�Ŷ�����...
	if (m_lpPlaySDL != NULL) {
		m_lpPlaySDL->PushPacket(cur_cache_ms, strFrame, pt_type, is_key, ts_ms);
	}
	// ��ӡ��Ͷ�ݵ���������֡��Ϣ...
	//blog(LOG_INFO, "%s Frame => Type: %d, Key: %d, PTS: %lu, Size: %d, PlaySeq: %lu, CircleSize: %d", TM_RECV_NAME,
	//     pt_type, is_key, ts_ms, strFrame.size(), nMaxPlaySeq, cur_circle.size/nPerPackSize );
	// �޸���Ϣ״̬ => �Ѿ���ȡ��������Ƶ����֡��������Ϣ...
	m_bNeedSleep = false;
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPMultiRecvThread::doSleepTo()
{
	// ���������Ϣ��ֱ�ӷ���...
	if (!m_bNeedSleep)
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = os_gettime_ns();
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

bool CUDPMultiRecvThread::doVolumeEvent(bool bIsVolPlus)
{
	if (m_lpPlaySDL == NULL) return false;
	return m_lpPlaySDL->doVolumeEvent(bIsVolPlus);
}