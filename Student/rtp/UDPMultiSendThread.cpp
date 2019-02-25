
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
	// 初始化音视频环形队列，预分配空间...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	circlebuf_init(&m_ex_audio_circle);
	// 初始化环形队列互斥保护对象...
	pthread_mutex_init_value(&m_MutexBuff);
	pthread_mutex_init_value(&m_MutexSock);
}

CUDPMultiSendThread::~CUDPMultiSendThread()
{
	// 必须关闭UDPSocket对象...
	this->ClearAllSocket();
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 释放音视频环形队列空间...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
	// 释放套接字互斥保护对象...
	pthread_mutex_destroy(&m_MutexBuff);
	pthread_mutex_destroy(&m_MutexSock);
	blog(LOG_INFO, "%s == [~CUDPMultiSendThread Exit] ==", TM_RECV_NAME);
}

void CUDPMultiSendThread::ClearAllSocket()
{
	// 对组播套接字进行互斥保护...
	pthread_mutex_lock(&m_MutexSock);
	if (m_lpUdpData != NULL) {
		delete m_lpUdpData;
		m_lpUdpData = NULL;
	}
	if (m_lpUdpLose != NULL) {
		delete m_lpUdpLose;
		m_lpUdpLose = NULL;
	}
	// 组播套接字互斥保护结束...
	pthread_mutex_unlock(&m_MutexSock);
}

BOOL CUDPMultiSendThread::InitThread(string & strHeader)
{
	// 保存上传的序列头命令包...
	m_strSeqHeader = strHeader;
	// 先删除已经存在的组播套接字对象...
	this->ClearAllSocket();
	// 重建组播数据发送对象...
	if (!this->BuildDataSocket())
		return false;
	// 重建组播丢包接收对象...
	if (!this->BuildLoseSocket())
		return false;
	// 启动组播处理线程...
	this->Start();
	// 返回执行结果...
	return true;
}

BOOL CUDPMultiSendThread::BuildDataSocket()
{
	// 获取组播地址、组播端口(房间号)等等...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwDataMultiAddr = inet_addr(DEF_MULTI_DATA_ADDR);
	// 重建组播套接字对象...
	ASSERT(m_lpUdpData == NULL);
	m_lpUdpData = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：这里的UDP组播套接字使用同步模式...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpData->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置重复使用端口...
	m_lpUdpData->ReuseAddr();
	// 设置发送和接收缓冲区...
	m_lpUdpData->SetSocketSendBufSize(128 * 1024);
	m_lpUdpData->SetSocketRecvBufSize(128 * 1024);
	// 设置TTL网络穿越数值...
	m_lpUdpData->SetMulticastTTL(32);
	m_lpUdpData->SetMulticastLoop(false);
	// 加入指定的组播组地址 => 发送接口使用默认接口...
	theErr = m_lpUdpData->JoinMulticastMember(dwDataMultiAddr, INADDR_ANY);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置组播发送的远程地址，在 SendTo() 当中可以简化接口...
	m_lpUdpData->SetRemoteAddr(DEF_MULTI_DATA_ADDR, nMultiPort);
	// 设置数据发送接口 => 多网卡时需要指定组播发送网络...
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nInterAddr = inet_addr(strIPSend.c_str());
	// 如果组播发送接口不是INADDR_ANY，才进行配置修改...
	if (nInterAddr != INADDR_ANY) {
		theErr = m_lpUdpData->ModMulticastInterface(nInterAddr);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}
	return true;
}

// 重置组播数据套接字的发送接口...
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
	// 获取组播地址、组播端口(房间号)等等...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwLoseMultiAddr = inet_addr(DEF_MULTI_LOSE_ADDR);
	// 重建组播套接字对象...
	ASSERT(m_lpUdpLose == NULL);
	m_lpUdpLose = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：这里的UDP组播套接字使用同步模式...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpLose->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置重复使用端口...
	m_lpUdpLose->ReuseAddr();
	// 接收者 => 绑定组播端口号码，绑定失败，返回错误...
	theErr = m_lpUdpLose->Bind(INADDR_ANY, nMultiPort);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置发送和接收缓冲区...
	m_lpUdpLose->SetSocketSendBufSize(128 * 1024);
	m_lpUdpLose->SetSocketRecvBufSize(128 * 1024);
	// 设置TTL网络穿越数值...
	m_lpUdpLose->SetMulticastTTL(32);
	m_lpUdpLose->SetMulticastLoop(false);
	// 加入指定的组播组地址 => 发送接口使用默认接口...
	theErr = m_lpUdpLose->JoinMulticastMember(dwLoseMultiAddr, INADDR_ANY);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置组播发送的远程地址，在 SendTo() 当中可以简化接口...
	m_lpUdpLose->SetRemoteAddr(DEF_MULTI_LOSE_ADDR, nMultiPort);
	return true;
}

void CUDPMultiSendThread::doSendHeaderCmd()
{
	if (m_lpUdpData == NULL || m_strSeqHeader.size() <= 0)
		return;
	// 每隔1秒发送一个探测命令包 => 必须转换成有符号...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// 第一次需要先发格式头数据包...
	if (m_next_header_ns < 0) {
		m_next_header_ns = cur_time_ns;
	}
	// 如果发包时间还没到，直接返回...
	if (m_next_header_ns > cur_time_ns)
		return;
	ASSERT(cur_time_ns >= m_next_header_ns);
	GM_Error theErr = m_lpUdpData->SendTo((void*)m_strSeqHeader.c_str(), m_strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 计算下次发送格式头命令的时间戳...
	m_next_header_ns = os_gettime_ns() + period_ns;
}

BOOL CUDPMultiSendThread::Transfer(char * lpBuffer, int inRecvLen)
{
	// 判断线程是否已经退出...
	if (this->IsStopRequested()) {
		blog(LOG_INFO, "Error => Multicast Thread has been stoped");
		return false;
	}
	// 判断传递的数据是否有效...
	if (lpBuffer == NULL || inRecvLen <= 0)
		return false;
	// 进行组播套接字互斥保护...
	pthread_mutex_lock(&m_MutexSock);
	// 每隔1秒中发送音视频格式头信息...
	this->doSendHeaderCmd();
	// 如果组播套接字有效，直接将得到的数据包转发到组播网络当中...
	GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)lpBuffer, inRecvLen) : GM_NoErr;
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 组播套接字互斥保护结束...
	pthread_mutex_unlock(&m_MutexSock);
	return true;
}

void CUDPMultiSendThread::PushPacket(string & strPacket, uint8_t inPType)
{
	// 进行组播缓存互斥保护...
	pthread_mutex_lock(&m_MutexBuff);
	// 输入缓存的长度一定是数据块的整数倍...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	ASSERT((strPacket.size() % nPerPackSize) == 0);
	// 根据标志定位到音频或视频环形队列，并将输入的缓存追加到环形队列当中...
	circlebuf & cur_circle = ((inPType == PT_TAG_EX_AUDIO) ? m_ex_audio_circle : ((inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle));
	circlebuf_push_back(&cur_circle, strPacket.c_str(), strPacket.size());
	// 遍历环形队列，删除所有超过n秒的缓存数据包 => 不管是否是关键帧或完整包，只是为补包而存在...
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    max_ts = 0;
	// 读取最大的数据包的内容，获取最大时间戳...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_ts = lpCurHeader->ts;
	// 遍历环形队列，查看是否有需要删除的数据包...
	while (cur_circle.size > 0) {
		// 读取第一个数据包的内容，获取最小时间戳...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// 计算环形队列当中总的组播缓存时间...
		uint32_t cur_buf_ms = max_ts - lpCurHeader->ts;
		// 如果总组播缓存时间不超过n秒，中断操作...
		if (cur_buf_ms <= MAX_MULTI_JAM_MS)
			break;
		assert(cur_buf_ms > MAX_MULTI_JAM_MS);
		// 如果总组播缓存时间超过n秒，删除最小数据包，继续寻找...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// 组播缓存互斥保护结束...
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
	// 调用接口从网络层获取数据包 => 这里是同步套接字，会阻塞一直等待返回...
	theErr = m_lpUdpLose->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// 注意：这里不用打印错误信息，没有收到数据就立即返回...
	if (theErr != GM_NoErr || outRecvLen <= 0)
		return;
	// 判断最大接收数据长度 => DEF_MTU_SIZE + rtp_hdr_t
	UInt32 nMaxSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (outRecvLen > nMaxSize) {
		blog(LOG_INFO, "[Recv-Error] Max => %lu, Addr => %lu:%d, Size => %lu", nMaxSize, outRemoteAddr, outRemotePort, outRecvLen);
		return;
	}
	// 通过第一个字节的低2位，判断终端类型...
	uint8_t tmTag = ioBuffer[0] & 0x03;
	// 获取第一个字节的中2位，得到终端身份...
	uint8_t idTag = (ioBuffer[0] >> 2) & 0x03;
	// 获取第一个字节的高4位，得到数据包类型...
	uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	// 注意：只处理 学生观看端 发出的补包命令...
	if (ptTag != PT_TAG_SUPPLY || tmTag != TM_TAG_STUDENT || idTag != ID_TAG_LOOKER)
		return;
	// 注意：组播学生接收端的补包从组播发送端的缓存获取...
	rtp_supply_t rtpSupply = { 0 };
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, ioBuffer, nHeadSize);
	if ((rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != outRecvLen))
		return;
	// 获取需要补包的序列号，加入到补包队列当中...
	char * lpDataPtr = ioBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while (nDataSize > 0) {
		// 获取补包序列号...
		uint32_t nLoseSeq = 0;
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// 移动数据区指针位置...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
		// 查看这个丢包号是否是服务器端也要补的包...
		// 服务器收到补包后会自动转发，这里就不用补了...
		if (m_lpServerThread->doIsServerLose(rtpSupply.suType, nLoseSeq)) {
			//blog(LOG_INFO, "%s Multicast ServerLose, Seq: %lu, PType: %d", TM_RECV_NAME, nLoseSeq, rtpSupply.suType);
			continue;
		}
		// 进行组播缓存互斥保护...
		pthread_mutex_lock(&m_MutexBuff);
		// 是观看端丢失的新包，需要进行补包队列处理...
		this->doSendLosePacket(rtpSupply.suType, nLoseSeq);
		// 组播缓存互斥保护结束...
		pthread_mutex_unlock(&m_MutexBuff);
	}
}
///////////////////////////////////////////////////////////////////////////////////
// 注意：服务器和发送端，都是假定环形队列里的序号是连续的，直接通过序号偏移进行定位...
///////////////////////////////////////////////////////////////////////////////////
void CUDPMultiSendThread::doSendLosePacket(uint8_t inPType, uint32_t inLoseSeq)
{
	// 根据数据包类型，找到缓存集合...
	circlebuf & cur_circle = ((inPType == PT_TAG_EX_AUDIO) ? m_ex_audio_circle : ((inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle));
	// 如果环形队列为空，直接返回...
	if (cur_circle.size <= 0)
		return;
	// 先找到环形队列中最前面数据包的头指针 => 最小序号...
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	uint32_t max_seq = 0, min_seq = 0;
	// 找到环形队列当中最小包号和最大包号...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpFrontHeader->seq;
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpFrontHeader->seq;
	// 发生最大序号越界是有可能的 => 组合数据帧需要凑齐...
	if (max_seq < inLoseSeq) {
		blog(LOG_INFO, "%s Multicast MaxSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	// 如果要补充的数据包序号比最小序号还要小 => 没有找到，直接返回...
	if (inLoseSeq < min_seq) {
		blog(LOG_INFO, "%s Multicast MinSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	ASSERT(inLoseSeq >= min_seq);
	// 注意：环形队列当中的序列号一定是连续的...
	// 两者之差就是要发送数据包的头指针位置...
	nSendPos = (inLoseSeq - min_seq) * nPerPackSize;
	// 如果补包位置大于或等于环形队列长度 => 补包越界...
	if (nSendPos >= cur_circle.size) {
		blog(LOG_INFO, "%s Multicast Position Error, CurSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
	// 获取将要发送数据包的包头位置和有效数据长度...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// 如果找到的序号位置不对 或 本身就是需要补的丢包...
	if ((lpSendHeader->pt == PT_TAG_LOSE) || (lpSendHeader->seq != inLoseSeq)) {
		blog(LOG_INFO, "%s Multicast Supply Error, CurSeq: %lu, LoseSeq: %lu, PType: %d", TM_RECV_NAME, lpSendHeader->seq, inLoseSeq, inPType);
		return;
	}
	// 获取有效的数据区长度 => 包头 + 数据...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// 如果组播套接字有效，将补充数据包转发到组播网络当中...
	GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)szPacketBuffer, nSendSize) : GM_NoErr;
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 打印已经成功补包 => 组播丢失的数据包序号...
	blog(LOG_INFO, "%s Multicast SendLose OK, CurSeq: [%lu, %lu], LoseSeq: %lu, PType: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, inPType);
}

///////////////////////////////////////////////////////////////////////////////////
// 注意：采用了新的模型 => 假定环形队列里的序号不是连续的，需要逐个查询序号...
// 注意：服务器和发送端，都是假定环形队列里的序号是连续的，直接通过序号偏移进行定位...
///////////////////////////////////////////////////////////////////////////////////
/*void CUDPMultiSendThread::doSendLosePacket(bool bIsAudio, uint32_t inLoseSeq)
{
	// 根据数据包类型，找到缓存集合...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	// 如果环形队列为空，直接返回...
	if (cur_circle.size <= 0)
		return;
	// 先找到环形队列中最前面数据包的头指针 => 最小序号...
	rtp_hdr_t * lpFrontHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	uint32_t max_seq = 0, min_seq = 0;
	// 如果环形队列最大序号包，比丢包号小 => 补包越界，直接返回...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpFrontHeader->seq;
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpFrontHeader->seq;
	// 发生最大序号越界是有可能的 => 组合数据帧需要凑齐...
	if (max_seq < inLoseSeq) {
		blog(LOG_INFO, "%s Multicast MaxSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
		return;
	}
	// 遍历环形队列，查看补包序号是否在环形队列当中...
	while (cur_circle.size > nSendPos) {
		// 从环形队列当中读取一个完整数据块 => 从头开始读取...
		circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果要补充的数据包序号比当前最小序号小 => 补包序号过期，发生了数据混乱...
		if (inLoseSeq < lpFrontHeader->seq) {
			blog(LOG_INFO, "%s Multicast MinSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
			return;
		}
		// 如果要补充的数据包序号比当前最小序号大 => 移动序号，继续查找...
		if (inLoseSeq > lpFrontHeader->seq) {
			nSendPos += nPerPackSize;
			continue;
		}
		// 如果要补充的数据包序号等于当前最小序号 => 正确找到补包序号...
		ASSERT(inLoseSeq == lpFrontHeader->seq);
		// 如果找到的序号本身就是需要补的丢包...
		if (lpFrontHeader->pt == PT_TAG_LOSE)
			return;
		// 获取有效的数据区长度 => 包头 + 数据...
		nSendSize = sizeof(rtp_hdr_t) + lpFrontHeader->psize;
		// 如果组播套接字有效，将补充数据包转发到组播网络当中...
		GM_Error theErr = (m_lpUdpData != NULL) ? m_lpUdpData->SendTo((void*)szPacketBuffer, nSendSize) : GM_NoErr;
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
		// 打印已经成功补包 => 组播丢失的数据包序号...
		blog(LOG_INFO, "%s Multicast SendLose OK, CurSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
		return;
	}
	// 遍历了所有的环形队列，还是没有找到指定的丢包序号...
	blog(LOG_INFO, "%s Multicast Can't Find CurSeq: [%lu, %lu], LoseSeq: %lu, Audio: %d", TM_RECV_NAME, min_seq, max_seq, inLoseSeq, bIsAudio);
}*/