
#include "UDPSocket.h"
#include "student-app.h"
#include "UDPRecvThread.h"
#include "UDPMultiSendThread.h"

CUDPMultiSendThread::CUDPMultiSendThread(CUDPRecvThread * lpServerThread)
  : m_lpServerThread(lpServerThread)
  , m_next_header_ns(-1)
  , m_lpUdpData(NULL)
  , m_lpUdpLose(NULL)
{
	ASSERT(m_lpServerThread != NULL);
	// ��ʼ������Ƶ���ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	circlebuf_init(&m_ex_audio_circle);
	// ��ʼ�����ζ��л��Ᵽ������...
	pthread_mutex_init_value(&m_MutexBuff);
	pthread_mutex_init_value(&m_MutexSock);
}

CUDPMultiSendThread::~CUDPMultiSendThread()
{
	// ����ر�UDPSocket����...
	this->ClearAllSocket();
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// �ͷ�����Ƶ���ζ��пռ�...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
	// �ͷ��׽��ֻ��Ᵽ������...
	pthread_mutex_destroy(&m_MutexBuff);
	pthread_mutex_destroy(&m_MutexSock);
	blog(LOG_INFO, "%s == [~CUDPMultiSendThread Exit] ==", TM_RECV_NAME);
}

void CUDPMultiSendThread::ClearAllSocket()
{
	// ���鲥�׽��ֽ��л��Ᵽ��...
	pthread_mutex_lock(&m_MutexSock);
	if (m_lpUdpData != NULL) {
		delete m_lpUdpData;
		m_lpUdpData = NULL;
	}
	if (m_lpUdpLose != NULL) {
		delete m_lpUdpLose;
		m_lpUdpLose = NULL;
	}
	// �鲥�׽��ֻ��Ᵽ������...
	pthread_mutex_unlock(&m_MutexSock);
}

BOOL CUDPMultiSendThread::InitThread(string & strHeader)
{
	// �����ϴ�������ͷ�����...
	m_strSeqHeader = strHeader;
	// ��ɾ���Ѿ����ڵ��鲥�׽��ֶ���...
	this->ClearAllSocket();
	// �ؽ��鲥���ݷ��Ͷ���...
	if (!this->BuildDataSocket())
		return false;
	// �ؽ��鲥�������ն���...
	if (!this->BuildLoseSocket())
		return false;
	// �����鲥�����߳�...
	this->Start();
	// ����ִ�н��...
	return true;
}

BOOL CUDPMultiSendThread::BuildDataSocket()
{
	// ��ȡ�鲥��ַ���鲥�˿�(�����)�ȵ�...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwDataMultiAddr = inet_addr(DEF_MULTI_DATA_ADDR);
	// �ؽ��鲥�׽��ֶ���...
	ASSERT(m_lpUdpData == NULL);
	m_lpUdpData = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺�����UDP�鲥�׽���ʹ��ͬ��ģʽ...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpData->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����ظ�ʹ�ö˿�...
	m_lpUdpData->ReuseAddr();
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
	// �������ݷ��ͽӿ� => ������ʱ��Ҫָ���鲥��������...
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nInterAddr = inet_addr(strIPSend.c_str());
	// ����鲥���ͽӿڲ���INADDR_ANY���Ž��������޸�...
	if (nInterAddr != INADDR_ANY) {
		theErr = m_lpUdpData->ModMulticastInterface(nInterAddr);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}
	return true;
}

// �����鲥�����׽��ֵķ��ͽӿ�...
void CUDPMultiSendThread::doResetMulticastIPSend()
{
	if (m_lpUdpData == NULL)
		return;
	GM_Error theErr = GM_NoErr;
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nMultiInterAddr = inet_addr(strIPSend.c_str());
	theErr = m_lpUdpData->ModMulticastInterface(nMultiInterAddr);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
}

BOOL CUDPMultiSendThread::BuildLoseSocket()
{
	// ��ȡ�鲥��ַ���鲥�˿�(�����)�ȵ�...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwLoseMultiAddr = inet_addr(DEF_MULTI_LOSE_ADDR);
	// �ؽ��鲥�׽��ֶ���...
	ASSERT(m_lpUdpLose == NULL);
	m_lpUdpLose = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺�����UDP�鲥�׽���ʹ��ͬ��ģʽ...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpLose->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// �����ظ�ʹ�ö˿�...
	m_lpUdpLose->ReuseAddr();
	// ������ => ���鲥�˿ں��룬��ʧ�ܣ����ش���...
	theErr = m_lpUdpLose->Bind(INADDR_ANY, nMultiPort);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
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
	return true;
}

void CUDPMultiSendThread::doSendHeaderCmd()
{
	if (m_lpUdpData == NULL || m_strSeqHeader.size() <= 0)
		return;
	// ÿ��1�뷢��һ��̽������� => ����ת�����з���...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// ��һ����Ҫ�ȷ���ʽͷ���ݰ�...
	if (m_next_header_ns < 0) {
		m_next_header_ns = cur_time_ns;
	}
	// �������ʱ�仹û����ֱ�ӷ���...
	if (m_next_header_ns > cur_time_ns)
		return;
	ASSERT(cur_time_ns >= m_next_header_ns);
	GM_Error theErr = m_lpUdpData->SendTo((void*)m_strSeqHeader.c_str(), m_strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �����´η��͸�ʽͷ�����ʱ���...
	m_next_header_ns = os_gettime_ns() + period_ns;
}

BOOL CUDPMultiSendThread::Transfer(char * lpBuffer, int inRecvLen)
{
	// �ж��߳��Ƿ��Ѿ��˳�...
	if (this->IsStopRequested()) {
		blog(LOG_INFO, "Error => Multicast Thread has been stoped");
		return false;
	}
	// �жϴ��ݵ������Ƿ���Ч...
	if (lpBuffer == NULL || inRecvLen <= 0)
		return false;
	// �����鲥�׽��ֻ��Ᵽ��...
	pthread_mutex_lock(&m_MutexSock);
	// ÿ��1���з�������Ƶ��ʽͷ��Ϣ...
	this->doSendHeaderCmd();
	// ����鲥�׽�����Ч��ֱ�ӽ��õ������ݰ�ת�����鲥���統��...
	GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)lpBuffer, inRecvLen) : GM_NoErr;
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �鲥�׽��ֻ��Ᵽ������...
	pthread_mutex_unlock(&m_MutexSock);
	return true;
}

void CUDPMultiSendThread::PushPacket(string & strPacket, uint8_t inPType)
{
	// �����鲥���滥�Ᵽ��...
	pthread_mutex_lock(&m_MutexBuff);
	// ���뻺��ĳ���һ�������ݿ��������...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	ASSERT((strPacket.size() % nPerPackSize) == 0);
	// ���ݱ�־��λ����Ƶ����Ƶ���ζ��У���������Ļ���׷�ӵ����ζ��е���...
	circlebuf & cur_circle = ((inPType == PT_TAG_EX_AUDIO) ? m_ex_audio_circle : ((inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle));
	circlebuf_push_back(&cur_circle, strPacket.c_str(), strPacket.size());
	// �������ζ��У�ɾ�����г���n��Ļ������ݰ� => �����Ƿ��ǹؼ�֡����������ֻ��Ϊ����������...
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    max_ts = 0;
	// ��ȡ�������ݰ������ݣ���ȡ���ʱ���...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_ts = lpCurHeader->ts;
	// �������ζ��У��鿴�Ƿ�����Ҫɾ�������ݰ�...
	while (cur_circle.size > 0) {
		// ��ȡ��һ�����ݰ������ݣ���ȡ��Сʱ���...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// ���㻷�ζ��е����ܵ��鲥����ʱ��...
		uint32_t cur_buf_ms = max_ts - lpCurHeader->ts;
		// ������鲥����ʱ�䲻����n�룬�жϲ���...
		if (cur_buf_ms <= MAX_MULTI_JAM_MS)
			break;
		assert(cur_buf_ms > MAX_MULTI_JAM_MS);
		// ������鲥����ʱ�䳬��n�룬ɾ����С���ݰ�������Ѱ��...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// �鲥���滥�Ᵽ������...
	pthread_mutex_unlock(&m_MutexBuff);
}

void CUDPMultiSendThread::Entry()
{
	blog(LOG_INFO, "== CUDPMultiSendThread::Entry() ==");
	while (!this->IsStopRequested() && m_lpUdpLose != NULL) {
		this->doRecvPacket();
	}
}

void CUDPMultiSendThread::doRecvPacket()
{
	if (m_lpUdpLose == NULL)
		return;
	GM_Error theErr = GM_NoErr;
	UInt32   outRecvLen = 0;
	UInt32   outRemoteAddr = 0;
	UInt16   outRemotePort = 0;
	UInt32   inBufLen = MAX_BUFF_LEN;
	char     ioBuffer[MAX_BUFF_LEN] = { 0 };
	// ���ýӿڴ�������ȡ���ݰ� => ������ͬ���׽��֣�������һֱ�ȴ�����...
	theErr = m_lpUdpLose->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// ע�⣺���ﲻ�ô�ӡ������Ϣ��û���յ����ݾ���������...
	if (theErr != GM_NoErr || outRecvLen <= 0)
		return;
	// �ж����������ݳ��� => DEF_MTU_SIZE + rtp_hdr_t
	UInt32 nMaxSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (outRecvLen > nMaxSize) {
		blog(LOG_INFO, "[Recv-Error] Max => %lu, Addr => %lu:%d, Size => %lu", nMaxSize, outRemoteAddr, outRemotePort, outRecvLen);
		return;
	}
	// ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
	uint8_t tmTag = ioBuffer[0] & 0x03;
	// ��ȡ��һ���ֽڵ���2λ���õ��ն����...
	uint8_t idTag = (ioBuffer[0] >> 2) & 0x03;
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
	uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	// ע�⣺ֻ���� ѧ���ۿ��� �����Ĳ�������...
	if (ptTag != PT_TAG_SUPPLY || tmTag != TM_TAG_STUDENT || idTag != ID_TAG_LOOKER)
		return;
	// ע�⣺�鲥ѧ�����ն˵Ĳ������鲥���Ͷ˵Ļ����ȡ...
	rtp_supply_t rtpSupply = { 0 };
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, ioBuffer, nHeadSize);
	if ((rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != outRecvLen))
		return;
	// ��ȡ��Ҫ���������кţ����뵽�������е���...
	char * lpDataPtr = ioBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while (nDataSize > 0) {
		// ��ȡ�������к�...
		uint32_t nLoseSeq = 0;
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// �ƶ�������ָ��λ��...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
		// �鿴����������Ƿ��Ƿ�������ҲҪ���İ�...
		// �������յ���������Զ�ת��������Ͳ��ò���...
		if (m_lpServerThread->doIsServerLose(rtpSupply.suType, nLoseSeq)) {
			//blog(LOG_INFO, "%s Multicast ServerLose, Seq: %lu, PType: %d", TM_RECV_NAME, nLoseSeq, rtpSupply.suType);
			continue;
		}
		// �����鲥���滥�Ᵽ��...
		pthread_mutex_lock(&m_MutexBuff);
		// �ǹۿ��˶�ʧ���°�����Ҫ���в������д���...
		this->doSendLosePacket(rtpSupply.suType, nLoseSeq);
		// �鲥���滥�Ᵽ������...
		pthread_mutex_unlock(&m_MutexBuff);
	}
}
///////////////////////////////////////////////////////////////////////////////////
// ע�⣺�������ͷ��Ͷˣ����Ǽٶ����ζ����������������ģ�ֱ��ͨ�����ƫ�ƽ��ж�λ...
///////////////////////////////////////////////////////////////////////////////////
void CUDPMultiSendThread::doSendLosePacket(uint8_t inPType, uint32_t inLoseSeq)
{
	// �������ݰ����ͣ��ҵ����漯��...
	circlebuf & cur_circle = ((inPType == PT_TAG_EX_AUDIO) ? m_ex_audio_circle : ((inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle));
	// ������ζ���Ϊ�գ�ֱ�ӷ���...
	if (cur_circle.size <= 0)
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	uint32_t max_seq = 0, min_seq = 0;
	// �ҵ����ζ��е�����С���ź�������...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpFrontHeader->seq;
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpFrontHeader->seq;
	// ����������Խ�����п��ܵ� => �������֡��Ҫ����...
	if (max_seq < inLoseSeq) {
		blog(LOG_INFO, "%s Multicast MaxSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	// ���Ҫ��������ݰ���ű���С��Ż�ҪС => û���ҵ���ֱ�ӷ���...
	if (inLoseSeq < min_seq) {
		blog(LOG_INFO, "%s Multicast MinSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	ASSERT(inLoseSeq >= min_seq);
	// ע�⣺���ζ��е��е����к�һ����������...
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (inLoseSeq - min_seq) * nPerPackSize;
	// �������λ�ô��ڻ���ڻ��ζ��г��� => ����Խ��...
	if (nSendPos >= cur_circle.size) {
		blog(LOG_INFO, "%s Multicast Position Error, CurSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
	// ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// ����ҵ������λ�ò��� �� ���������Ҫ���Ķ���...
	if ((lpSendHeader->pt == PT_TAG_LOSE) || (lpSendHeader->seq != inLoseSeq)) {
		blog(LOG_INFO, "%s Multicast Supply Error, CurSeq: %lu, LoseSeq: %lu, PType: %d", TM_RECV_NAME, lpSendHeader->seq, inLoseSeq, inPType);
		return;
	}
	// ��ȡ��Ч������������ => ��ͷ + ����...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// ����鲥�׽�����Ч�����������ݰ�ת�����鲥���統��...
	GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)szPacketBuffer, nSendSize) : GM_NoErr;
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�Ѿ��ɹ����� => �鲥��ʧ�����ݰ����...
	blog(LOG_INFO, "%s Multicast SendLose OK, CurSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
}

///////////////////////////////////////////////////////////////////////////////////
// ע�⣺�������µ�ģ�� => �ٶ����ζ��������Ų��������ģ���Ҫ�����ѯ���...
// ע�⣺�������ͷ��Ͷˣ����Ǽٶ����ζ����������������ģ�ֱ��ͨ�����ƫ�ƽ��ж�λ...
///////////////////////////////////////////////////////////////////////////////////
/*void CUDPMultiSendThread::doSendLosePacket(bool bIsAudio, uint32_t inLoseSeq)
{
	// �������ݰ����ͣ��ҵ����漯��...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	// ������ζ���Ϊ�գ�ֱ�ӷ���...
	if (cur_circle.size <= 0)
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	rtp_hdr_t * lpFrontHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	uint32_t max_seq = 0, min_seq = 0;
	// ������ζ��������Ű����ȶ�����С => ����Խ�磬ֱ�ӷ���...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpFrontHeader->seq;
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpFrontHeader->seq;
	// ����������Խ�����п��ܵ� => �������֡��Ҫ����...
	if (max_seq < inLoseSeq) {
		blog(LOG_INFO, "%s Multicast MaxSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
		return;
	}
	// �������ζ��У��鿴��������Ƿ��ڻ��ζ��е���...
	while (cur_circle.size > nSendPos) {
		// �ӻ��ζ��е��ж�ȡһ���������ݿ� => ��ͷ��ʼ��ȡ...
		circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// ���Ҫ��������ݰ���űȵ�ǰ��С���С => ������Ź��ڣ����������ݻ���...
		if (inLoseSeq < lpFrontHeader->seq) {
			blog(LOG_INFO, "%s Multicast MinSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
			return;
		}
		// ���Ҫ��������ݰ���űȵ�ǰ��С��Ŵ� => �ƶ���ţ���������...
		if (inLoseSeq > lpFrontHeader->seq) {
			nSendPos += nPerPackSize;
			continue;
		}
		// ���Ҫ��������ݰ���ŵ��ڵ�ǰ��С��� => ��ȷ�ҵ��������...
		ASSERT(inLoseSeq == lpFrontHeader->seq);
		// ����ҵ�����ű��������Ҫ���Ķ���...
		if (lpFrontHeader->pt == PT_TAG_LOSE)
			return;
		// ��ȡ��Ч������������ => ��ͷ + ����...
		nSendSize = sizeof(rtp_hdr_t) + lpFrontHeader->psize;
		// ����鲥�׽�����Ч�����������ݰ�ת�����鲥���統��...
		GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)szPacketBuffer, nSendSize) : GM_NoErr;
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
		// ��ӡ�Ѿ��ɹ����� => �鲥��ʧ�����ݰ����...
		blog(LOG_INFO, "%s Multicast SendLose OK, CurSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
		return;
	}
	// ���������еĻ��ζ��У�����û���ҵ�ָ���Ķ������...
	blog(LOG_INFO, "%s Multicast Can't Find CurSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
}*/