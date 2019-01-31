
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
	// 初始化命令状态...
	m_nCmdState = kCmdSendCreate;
	// 初始化rtp序列头结构体...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_supply, 0, sizeof(m_rtp_supply));
	// 初始化补包命令格式信息...
	m_rtp_supply.tm = TM_TAG_STUDENT;
	m_rtp_supply.id = ID_TAG_LOOKER;
	m_rtp_supply.pt = PT_TAG_SUPPLY;
	// 初始化音视频环形队列，预分配空间...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	// 初始化超时0点时刻点...
	m_time_zero_ns = os_gettime_ns();
}

CUDPMultiRecvThread::~CUDPMultiRecvThread()
{
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 关闭UDPSocket对象...
	this->ClearAllSocket();
	// 删除音视频播放线程...
	this->ClosePlayer();
	// 释放音视频环形队列空间...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
	// 通知左侧窗口，拉流线程已经停止...
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
	// 先删除已经存在的组播套接字对象...
	this->ClearAllSocket();
	// 重建组播数据接收对象...
	if (!this->BuildDataSocket())
		return false;
	// 重建组播丢包发送对象...
	if (!this->BuildLoseSocket())
		return false;
	// 启动组播处理线程...
	this->Start();
	// 返回执行结果...
	return true;
}

BOOL CUDPMultiRecvThread::BuildDataSocket()
{
	// 获取组播地址、组播端口(房间号)等等...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwDataMultiAddr = inet_addr(DEF_MULTI_DATA_ADDR);
	// 重建组播套接字对象...
	ASSERT(m_lpUdpData == NULL);
	m_lpUdpData = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：这里的UDP组播套接字使用异步模式...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpData->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置异步模式...
	m_lpUdpData->NonBlocking();
	// 设置重复使用端口...
	m_lpUdpData->ReuseAddr();
	// 接收者 => 绑定组播端口号码，绑定失败，返回错误...
	theErr = m_lpUdpData->Bind(INADDR_ANY, nMultiPort);
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
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
	return true;
}

BOOL CUDPMultiRecvThread::BuildLoseSocket()
{
	// 获取组播地址、组播端口(房间号)等等...
	int  nMultiPort = atoi(App()->GetRoomIDStr().c_str());
	UINT dwLoseMultiAddr = inet_addr(DEF_MULTI_LOSE_ADDR);
	// 重建组播套接字对象...
	ASSERT(m_lpUdpLose == NULL);
	m_lpUdpLose = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：这里的UDP组播套接字使用异步模式...
	///////////////////////////////////////////////////////
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUdpLose->Open();
	if (theErr != GM_NoErr) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置异步模式...
	m_lpUdpData->NonBlocking();
	// 设置重复使用端口...
	m_lpUdpLose->ReuseAddr();
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
	// 设置丢包发送接口 => 多网卡时需要指定组播发送网络...
	string & strIPSend = App()->GetMultiIPSendAddr();
	UINT nInterAddr = inet_addr(strIPSend.c_str());
	// 如果组播发送接口不是INADDR_ANY，才进行配置修改...
	if (nInterAddr != INADDR_ANY) {
		theErr = m_lpUdpLose->ModMulticastInterface(nInterAddr);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}
	return true;
}

// 重置组播丢包套接字的发送接口...
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
	// 更新显示窗口的文字信息内容 => 等待组播数据到达...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WatiMulticast"));
	// 执行线程函数，直到线程退出...
	while (!this->IsStopRequested()) {
		// 设置休息标志 => 只要有发包或收包就不能休息...
		m_bNeedSleep = true;
		// 检测组播接收是否发生超时...
		this->doCheckRecvTimeout();
		// 接收一个到达的服务器反馈包...
		this->doRecvPacket();
		// 先发送音频补包命令...
		this->doSendSupplyCmd(true);
		// 再发送视频补包命令...
		this->doSendSupplyCmd(false);
		// 从环形队列中抽取完整一帧音频放入播放器...
		this->doParseFrame(true);
		// 从环形队列中抽取完整一帧视频放入播放器...
		this->doParseFrame(false);
		// 等待发送或接收下一个数据包...
		this->doSleepTo();
	}
}

void CUDPMultiRecvThread::doCheckRecvTimeout()
{
	// 如果组播接收没有发生超时，直接返回，继续等待下次超时检测...
	uint32_t nDeltaTime = (uint32_t)((os_gettime_ns() - m_time_zero_ns)/1000000);
	if (nDeltaTime < MAX_MULTI_JAM_MS)
		return;
	// 重置超时时间0点检测时间戳...
	m_time_zero_ns = os_gettime_ns();
	ASSERT(nDeltaTime >= MAX_MULTI_JAM_MS);
	// 发生组播接收超时，需要重置相关变量...
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
	// 重置序列头格式信息...
	m_strSeqHeader.clear();
	m_strSPS.clear();
	m_strPPS.clear();
	// 初始化rtp序列头结构体...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	// 初始化音视频环形队列，预分配空间...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	// 清空补包序号队列...
	m_AudioMapLose.clear();
	m_VideoMapLose.clear();
	// 删除音视频播放线程...
	this->ClosePlayer();
	// 通知左侧窗口，拉流线程已经停止...
	App()->onUdpRecvThreadStop();
	// 更新显示窗口的文字信息内容 => 等待组播数据到达...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WatiMulticast"));
	// 打印超时信息...
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
	// 调用接口从网络层获取数据包 => 这里是异步套接字，会立即返回 => 不管错误...
	theErr = m_lpUdpData->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// 注意：这里不用打印错误信息，没有收到数据就立即返回...
	if (theErr != GM_NoErr || outRecvLen <= 0)
		return;
	// 修改休息状态 => 已经成功收包，不能休息...
	m_bNeedSleep = false;
	// 判断最大接收数据长度 => DEF_MTU_SIZE + rtp_hdr_t
	UInt32 nMaxSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (outRecvLen > nMaxSize) {
		blog(LOG_INFO, "[Recv-Error] Max => %lu, Addr => %lu:%d, Size => %lu", nMaxSize, outRemoteAddr, outRemotePort, outRecvLen);
		return;
	}
	// 重置超时0点时刻点...
	m_time_zero_ns = os_gettime_ns();
	// 获取第一个字节的高4位，得到数据包类型...
	uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	// 对收到命令包进行类型分发...
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
	// 通过 rtp_header_t 做为载体发送过来的 => 服务器直接原样转发的学生推流端的序列头结构体...
	if (m_lpUdpData == NULL || lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_header_t))
		return;
	// 如果已经收到了序列头，直接返回...
	if (m_nCmdState != kCmdSendCreate || m_rtp_header.pt == PT_TAG_HEADER || m_rtp_header.hasAudio || m_rtp_header.hasVideo)
		return;
	// 通过第一个字节的低2位，判断终端类型...
	uint8_t tmTag = lpBuffer[0] & 0x03;
	// 获取第一个字节的中2位，得到终端身份...
	uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
	// 获取第一个字节的高4位，得到数据包类型...
	uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果发送者不是 老师推流端 => 直接返回...
	if (tmTag != TM_TAG_TEACHER || idTag != ID_TAG_PUSHER)
		return;
	// 如果发现长度不够，直接返回...
	memcpy(&m_rtp_header, lpBuffer, sizeof(m_rtp_header));
	int nNeedSize = m_rtp_header.spsSize + m_rtp_header.ppsSize + sizeof(m_rtp_header);
	if (nNeedSize != inRecvLen) {
		blog(LOG_INFO, "%s Recv Header error, RecvLen: %d", TM_RECV_NAME, inRecvLen);
		memset(&m_rtp_header, 0, sizeof(m_rtp_header));
		return;
	}
	// 计算系统0点时刻...
	m_sys_zero_ns = os_gettime_ns();
	blog(LOG_INFO, "%s Set System Zero Time By Create => %I64d ms", TM_RECV_NAME, m_sys_zero_ns / 1000000);
	// 更新渲染界面，显示正在连接服务器的提示信息...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.ConnectServer"));
	// 直接将收到的序列头保存起来，方便组播转发使用...
	m_strSeqHeader.assign(lpBuffer, inRecvLen);
	// 读取SPS和PPS格式头信息...
	char * lpData = lpBuffer + sizeof(m_rtp_header);
	if (m_rtp_header.spsSize > 0) {
		m_strSPS.assign(lpData, m_rtp_header.spsSize);
		lpData += m_rtp_header.spsSize;
	}
	// 保存 PPS 格式头...
	if (m_rtp_header.ppsSize > 0) {
		m_strPPS.assign(lpData, m_rtp_header.ppsSize);
		lpData += m_rtp_header.ppsSize;
	}
	// 修改命令状态 => 开始向服务器准备就绪命令包...
	m_nCmdState = kCmdConnetOK;
	// 打印收到序列头结构体信息...
	blog(LOG_INFO, "%s Recv Header SPS: %d, PPS: %d", TM_RECV_NAME, m_strSPS.size(), m_strPPS.size());
	// 如果播放器已经创建，直接返回...
	if (m_lpPlaySDL != NULL || m_lpViewRender == NULL)
		return;
	// 更新渲染界面，已连接成功，等待接收网络数据...
	m_lpViewRender->doUpdateNotice(QTStr("Render.Window.WaitNetData"));
	// 新建播放器，初始化音视频线程...
	m_lpPlaySDL = new CPlaySDL(m_lpViewRender, m_sys_zero_ns);
	// 计算默认的网络缓冲评估时间 => 使用帧率来计算...
	m_server_cache_time_ms = 1000 / ((m_rtp_header.fpsNum > 0) ? m_rtp_header.fpsNum : 25);
	// 如果有视频，初始化视频线程...
	if (m_rtp_header.hasVideo) {
		int nPicFPS = m_rtp_header.fpsNum;
		int nPicWidth = m_rtp_header.picWidth;
		int nPicHeight = m_rtp_header.picHeight;
		m_lpPlaySDL->InitVideo(m_strSPS, m_strPPS, nPicWidth, nPicHeight, nPicFPS);
	}
	// 如果有音频，初始化音频线程...
	if (m_rtp_header.hasAudio) {
		int nInRateIndex = m_rtp_header.rateIndex;
		int nInChannelNum = m_rtp_header.channelNum;
		m_lpPlaySDL->InitAudio(nInRateIndex, nInChannelNum);
	}
}

void CUDPMultiRecvThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	// 如果没有收到序列头，不能进行数据处理...
	if (m_nCmdState != kCmdConnetOK)
		return;
	ASSERT(m_nCmdState >= kCmdConnetOK);
	// 如果输入的数据格式无效，直接返回...
	if (lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t))
		return;
	// 通过第一个字节的低2位，判断终端类型...
	uint8_t tmTag = lpBuffer[0] & 0x03;
	// 获取第一个字节的中2位，得到终端身份...
	uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
	// 获取第一个字节的高4位，得到数据包类型...
	uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 学生观看端，只接收来自自己的探测包，计算网络延时...
	if (tmTag == TM_TAG_STUDENT && idTag == ID_TAG_LOOKER) {
		// 获取收到的探测数据包...
		rtp_detect_t rtpDetect = { 0 };
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// 目前只处理来自服务器方向的探测结果...
		if (rtpDetect.dtDir != DT_TO_SERVER)
			return;
		ASSERT(rtpDetect.dtDir == DT_TO_SERVER);
		// 先处理服务器回传的音频最小序号包...
		this->doServerMinSeq(true, rtpDetect.maxAConSeq);
		// 再处理服务器回传的视频最小序号包...
		this->doServerMinSeq(false, rtpDetect.maxVConSeq);
		
		// 注意：网络延时是组播发送端已经计算完毕的数值...
		int keep_rtt = rtpDetect.tsSrc;
		// 如果音频和视频都没有丢包，直接设定最大重发次数为0...
		if (m_AudioMapLose.size() <= 0 && m_VideoMapLose.size() <= 0) {
			m_nMaxResendCount = 0;
		}
		// 防止网络突发延迟增大, 借鉴 TCP 的 RTT 遗忘衰减的算法...
		if( m_server_rtt_ms < 0 ) { m_server_rtt_ms = keep_rtt; }
		else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
		// 计算网络抖动的时间差值 => RTT的修正值...
		if( m_server_rtt_var_ms < 0 ) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
		else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
		// 计算缓冲评估时间 => 如果没有丢包，使用网络往返延时+网络抖动延时之和...
		if( m_nMaxResendCount > 0 ) {
			m_server_cache_time_ms = (2 * m_nMaxResendCount + 1) * (m_server_rtt_ms + m_server_rtt_var_ms) / 2;
		} else {
			m_server_cache_time_ms = m_server_rtt_ms + m_server_rtt_var_ms;
		}

		// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		blog(LOG_INFO, "%s Recv Detect => Dir: %d, dtNum: %d, rtt: %d ms, rtt_var: %d ms, cache_time: %d ms, ACircle: %d, VCircle: %d", TM_RECV_NAME,
			 rtpDetect.dtDir, rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms, m_server_cache_time_ms,
			 m_audio_circle.size/nPerPackSize, m_video_circle.size/nPerPackSize);
		// 打印播放器底层的缓存状态信息...
		/*if (m_lpPlaySDL != NULL) {
			blog(LOG_INFO, "%s Recv Detect => APacket: %d, VPacket: %d, AFrame: %d, VFrame: %d", TM_RECV_NAME,
				m_lpPlaySDL->GetAPacketSize(), m_lpPlaySDL->GetVPacketSize(), m_lpPlaySDL->GetAFrameSize(), m_lpPlaySDL->GetVFrameSize());
		}*/
	}
}

void CUDPMultiRecvThread::doServerMinSeq(bool bIsAudio, uint32_t inMinSeq)
{
	// 根据数据包类型，找到丢包集合、环形队列、最大播放包...
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	uint32_t  & nMaxPlaySeq = bIsAudio ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	// 如果输入的最小包号无效，直接返回...
	if (inMinSeq <= 0)
		return;
	// 丢包队列有效，才进行丢包查询工作...
	if (theMapLose.size() > 0) {
		// 遍历丢包队列，凡是小于服务器端最小序号的丢包，都要扔掉...
		GM_MapLose::iterator itorItem = theMapLose.begin();
		while (itorItem != theMapLose.end()) {
			rtp_lose_t & rtpLose = itorItem->second;
			// 如果要补的包号，不小于最小包号，可以补包...
			if (rtpLose.lose_seq >= inMinSeq) {
				itorItem++;
				continue;
			}
			// 如果要补的包号，比最小包号还要小，直接丢弃，已经过期了...
			theMapLose.erase(itorItem++);
		}
	}
	// 如果环形队列为空，无需清理，直接返回...
	if (cur_circle.size <= 0)
		return;
	// 获取环形队列当中最小序号和最大序号，找到清理边界...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_seq = 0, max_seq = 0;
	// 读取最小的数据包的内容，获取最小序列号...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpCurHeader->seq;
	// 读取最大的数据包的内容，获取最大序列号...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	// 如果环形队列中的最小序号包比服务器端的最小序号包大，不用清理缓存...
	if (min_seq >= inMinSeq)
		return;
	// 打印环形队列清理前的各个变量状态值...
	blog(LOG_INFO, "%s Clear => Audio: %d, ServerMin: %lu, MinSeq: %lu, MaxSeq: %lu, MaxPlaySeq: %lu", TM_RECV_NAME, bIsAudio, inMinSeq, min_seq, max_seq, nMaxPlaySeq);
	// 如果环形队列中的最大序号包比服务器端的最小序号包小，清理全部缓存...
	if (max_seq < inMinSeq) {
		inMinSeq = max_seq + 1;
	}
	// 缓存清理范围 => [min_seq, inMinSeq]...
	int nConsumeSize = (inMinSeq - min_seq) * nPerPackSize;
	circlebuf_pop_front(&cur_circle, NULL, nConsumeSize);
	// 将最大播放包设定为 => 最小序号包 - 1...
	nMaxPlaySeq = inMinSeq - 1;
}

void CUDPMultiRecvThread::doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID)
{
	// 根据数据包类型，找到丢包集合...
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// 如果没有找到指定的序列号，直接返回...
	GM_MapLose::iterator itorItem = theMapLose.find(inSeqID);
	if (itorItem == theMapLose.end())
		return;
	// 删除检测到的丢包节点...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	theMapLose.erase(itorItem);
	// 打印已收到的补包信息，还剩下的未补包个数...
	//blog(LOG_INFO, "%s Supply Erase => LoseSeq: %lu, ResendCount: %lu, LoseSize: %lu, Type: %d", TM_RECV_NAME, inSeqID, nResendCount, theMapLose.size(), inPType);
}

// 给丢失数据包预留环形队列缓存空间...
void CUDPMultiRecvThread::doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID)
{
	// 根据数据包类型，找到丢包集合...
	circlebuf & cur_circle = (inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// 需要对网络抖动时间差进行线路选择 => 只有一条服务器补包路线...
	int cur_rtt_var_ms = m_server_rtt_var_ms;
	// 准备数据包结构体并进行初始化 => 连续丢包，设置成相同的重发时间点，否则，会连续发非常多的补包命令...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	uint32_t sup_id = nStartLoseID;
	rtp_hdr_t rtpDis = { 0 };
	rtpDis.pt = PT_TAG_LOSE;
	// 注意：是闭区间 => [nStartLoseID, nEndLoseID]
	while (sup_id <= nEndLoseID) {
		// 给当前丢包预留缓冲区...
		rtpDis.seq = sup_id;
		circlebuf_push_back(&cur_circle, &rtpDis, sizeof(rtpDis));
		circlebuf_push_back_zero(&cur_circle, DEF_MTU_SIZE);
		// 将丢包序号加入丢包队列当中 => 毫秒时刻点...
		rtp_lose_t rtpLose = { 0 };
		rtpLose.resend_count = 0;
		rtpLose.lose_seq = sup_id;
		rtpLose.lose_type = inPType;
		// 注意：需要对网络抖动时间差进行线路选择 => 目前只有一条服务器补包路线...
		// 注意：这里要避免 网络抖动时间差 为负数的情况 => 还没有完成第一次探测的情况，也不能为0，会猛烈发包...
		// 重发时间点 => cur_time + rtt_var => 丢包时的当前时间 + 丢包时的网络抖动时间差 => 避免不是丢包，只是乱序的问题...
		rtpLose.resend_time = cur_time_ms + max(cur_rtt_var_ms, MAX_SLEEP_MS);
		theMapLose[sup_id] = rtpLose;
		// 打印已丢包信息，丢包队列长度...
		//blog(LOG_INFO, "%s Lose Seq: %lu, LoseSize: %lu, Type: %d", TM_RECV_NAME, sup_id, theMapLose.size(), inPType);
		// 累加当前丢包序列号...
		++sup_id;
	}
}

void CUDPMultiRecvThread::doTagAVPackProcess(char * lpBuffer, int inRecvLen)
{
	// 如果没有收到序列头，不能进行数据处理...
	if (m_nCmdState != kCmdConnetOK)
		return;
	ASSERT(m_nCmdState >= kCmdConnetOK);
	// 判断输入数据包的有效性 => 不能小于数据包的头结构长度...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if (lpBuffer == NULL || inRecvLen < sizeof(rtp_hdr_t) || inRecvLen > nPerPackSize) {
		blog(LOG_INFO, "%s Error => RecvLen: %d, Max: %d", TM_RECV_NAME, inRecvLen, nPerPackSize);
		return;
	}
	// 如果收到的缓冲区长度不够 或 填充量为负数，直接丢弃...
	rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
	int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
	int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
	uint8_t  pt_tag = lpNewHeader->pt;
	uint32_t new_id = lpNewHeader->seq;
	uint32_t max_id = new_id;
	uint32_t min_id = new_id;
	// 出现打包错误，丢掉错误包，打印错误信息...
	if (inRecvLen != nDataSize || nZeroSize < 0) {
		blog(LOG_INFO, "%s Error => RecvLen: %d, DataSize: %d, ZeroSize: %d", TM_RECV_NAME, inRecvLen, nDataSize, nZeroSize);
		return;
	}
	// 音视频使用不同的打包对象和变量...
	uint32_t & nMaxPlaySeq = (pt_tag == PT_TAG_AUDIO) ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	bool   &  bFirstSeqSet = (pt_tag == PT_TAG_AUDIO) ? m_bFirstAudioSeq : m_bFirstVideoSeq;
	circlebuf & cur_circle = (pt_tag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	// 注意：观看端后接入时，最大播放包序号不是从0开始的...
	// 如果最大播放序列包是0，说明是第一个包，需要保存为最大播放包 => 当前包号 - 1 => 最大播放包是已删除包，当前包序号从1开始...
	if (nMaxPlaySeq <= 0 && !bFirstSeqSet) {
		bFirstSeqSet = true;
		nMaxPlaySeq = new_id - 1;
		blog(LOG_INFO, "%s First Packet => Seq: %lu, Key: %d, PTS: %lu, PStart: %d, Type: %d", TM_RECV_NAME, new_id, lpNewHeader->pk, lpNewHeader->ts, lpNewHeader->pst, pt_tag);
	}
	// 如果收到的补充包比当前最大播放包还要小 => 说明是多次补包的冗余包，直接扔掉...
	// 注意：即使相等也要扔掉，因为最大播放序号包本身已经投递到了播放层，已经被删除了...
	if (new_id <= nMaxPlaySeq) {
		//blog(LOG_INFO, "%s Supply Discard => Seq: %lu, MaxPlaySeq: %lu, Type: %d", TM_RECV_NAME, new_id, nMaxPlaySeq, pt_tag);
		return;
	}
	// 打印收到的音频数据包信息 => 包括缓冲区填充量 => 每个数据包都是统一大小 => rtp_hdr_t + slice + Zero
	//blog(LOG_INFO, "%s Size: %d, Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, ZeroSize: %d", TM_RECV_NAME, inRecvLen, lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->pst, lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
	// 首先，将当前包序列号从丢包队列当中删除...
	this->doEraseLoseSeq(pt_tag, new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：每个环形队列中的数据包大小是一样的 => rtp_hdr_t + slice + Zero
	//////////////////////////////////////////////////////////////////////////////////////////////////
	static char szPacketBuffer[nPerPackSize] = { 0 };
	// 如果环形队列为空 => 需要对丢包做提前预判并进行处理...
	if (cur_circle.size < nPerPackSize) {
		// 新到序号包与最大播放包之间有空隙，说明有丢包...
		// 丢包闭区间 => [nMaxPlaySeq + 1, new_id - 1]
		if (new_id >(nMaxPlaySeq + 1)) {
			this->doFillLosePack(pt_tag, nMaxPlaySeq + 1, new_id - 1);
		}
		// 把最新序号包直接追加到环形队列的最后面，如果与最大播放包之间有空隙，已经在前面的步骤中补充好了...
		// 先加入包头和数据内容...
		circlebuf_push_back(&cur_circle, lpBuffer, inRecvLen);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if (nZeroSize > 0) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// 打印新追加的序号包 => 不管有没有丢包，都要追加这个新序号包...
		//blog(LOG_INFO, "%s Max Seq: %lu, Cricle: Zero", TM_RECV_NAME, new_id);
		return;
	}
	// 环形队列中至少要有一个数据包...
	ASSERT(cur_circle.size >= nPerPackSize);
	// 获取环形队列中最小序列号...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
	min_id = lpMinHeader->seq;
	// 获取环形队列中最大序列号...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacketBuffer;
	max_id = lpMaxHeader->seq;
	// 发生丢包条件 => max_id + 1 < new_id
	// 丢包闭区间 => [max_id + 1, new_id - 1];
	if (max_id + 1 < new_id) {
		this->doFillLosePack(pt_tag, max_id + 1, new_id - 1);
	}
	// 如果是丢包或正常序号包，放入环形队列，返回...
	if (max_id + 1 <= new_id) {
		// 先加入包头和数据内容...
		circlebuf_push_back(&cur_circle, lpBuffer, inRecvLen);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if (nZeroSize > 0) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// 打印新加入的最大序号包...
		//blog(LOG_INFO, "%s Max Seq: %lu, Circle: %d", TM_RECV_NAME, new_id, m_circle.size/nPerPackSize-1);
		return;
	}
	// 如果是丢包后的补充包 => max_id > new_id
	if (max_id > new_id) {
		// 如果最小序号大于丢包序号 => 打印错误，直接丢掉这个补充包...
		if (min_id > new_id) {
			//blog(LOG_INFO, "%s Supply Discard => Seq: %lu, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, min_id, max_id, pt_tag);
			return;
		}
		// 最小序号不能比丢包序号小...
		ASSERT(min_id <= new_id);
		// 计算缓冲区更新位置...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// 将获取的数据内容更新到指定位置...
		circlebuf_place(&cur_circle, nPosition, lpBuffer, inRecvLen);
		// 打印补充包信息...
		//blog(LOG_INFO, "%s Supply Success => Seq: %lu, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, min_id, max_id, pt_tag);
		return;
	}
	// 如果是其它未知包，打印信息...
	//blog(LOG_INFO, "%s Multicast Unknown => Seq: %lu, Slice: %d, Min-Max: [%lu, %lu], Type: %d", TM_RECV_NAME, new_id, lpNewHeader->psize, min_id, max_id, pt_tag);
}

void CUDPMultiRecvThread::doSendSupplyCmd(bool bIsAudio)
{
	if (m_lpUdpLose == NULL)
		return;
	// 根据数据包类型，找到丢包集合...
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	// 如果丢包集合队列为空，直接返回...
	if (theMapLose.size() <= 0)
		return;
	ASSERT(theMapLose.size() > 0);
	// 定义最大的补包缓冲区...
	const int nHeadSize = sizeof(m_rtp_supply);
	const int nPerPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacket[nPerPackSize] = { 0 };
	char * lpData = szPacket + nHeadSize;
	// 获取当前时间的毫秒值 => 小于或等于当前时间的丢包都需要通知发送端再次发送...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	// 需要对网络往返延迟值进行线路选择 => 只有一条服务器线路...
	int cur_rtt_ms = m_server_rtt_ms;
	// 重置补报长度为0 => 重新计算需要补包的个数...
	m_rtp_supply.suType = bIsAudio ? PT_TAG_AUDIO : PT_TAG_VIDEO;
	m_rtp_supply.suSize = 0;
	int nCalcMaxResend = 0;
	// 遍历丢包队列，找出需要补包的丢包序列号...
	GM_MapLose::iterator itorItem = theMapLose.begin();
	while (itorItem != theMapLose.end()) {
		rtp_lose_t & rtpLose = itorItem->second;
		if (rtpLose.resend_time <= cur_time_ms) {
			// 如果补包缓冲超过设定的最大值，跳出循环 => 最多补包200个...
			if ((nHeadSize + m_rtp_supply.suSize) >= nPerPackSize)
				break;
			// 累加补包长度和指针，拷贝补包序列号...
			memcpy(lpData, &rtpLose.lose_seq, sizeof(uint32_t));
			m_rtp_supply.suSize += sizeof(uint32_t);
			lpData += sizeof(uint32_t);
			// 累加重发次数...
			++rtpLose.resend_count;
			// 计算最大丢包重发次数...
			nCalcMaxResend = max(nCalcMaxResend, rtpLose.resend_count);
			// 注意：同时发送的补包，下次也同时发送，避免形成多个散列的补包命令...
			// 注意：如果一个网络往返延时都没有收到补充包，需要再次发起这个包的补包命令...
			// 注意：这里要避免 网络抖动时间差 为负数的情况 => 还没有完成第一次探测的情况，也不能为0，会猛烈发包...
			// 修正下次重传时间点 => cur_time + rtt => 丢包时的当前时间 + 网络往返延迟值 => 需要进行线路选择...
			rtpLose.resend_time = cur_time_ms + max(cur_rtt_ms, MAX_SLEEP_MS);
			// 如果补包次数大于1，下次补包不要太快，追加一个休息周期..
			rtpLose.resend_time += ((rtpLose.resend_count > 1) ? MAX_SLEEP_MS : 0);
			//blog(LOG_INFO, "%s Multicast Lose Seq: %lu, Audio: %d", TM_RECV_NAME, rtpLose.lose_seq, bIsAudio);
		}
		// 累加丢包算子对象...
		++itorItem;
	}
	// 如果补充包缓冲为空，直接返回...
	if (m_rtp_supply.suSize <= 0)
		return;
	// 保存计算出来的最大丢包重发次数...
	m_nMaxResendCount = nCalcMaxResend;
	// 更新补包命令头内容块...
	memcpy(szPacket, &m_rtp_supply, nHeadSize);
	// 如果补包缓冲不为空，才进行补包命令发送...
	GM_Error theErr = GM_NoErr;
	int nDataSize = nHeadSize + m_rtp_supply.suSize;
	////////////////////////////////////////////////////////////////////////////////////////
	// 调用套接字接口，直接发送RTP数据包 => 根据发包线路进行有选择的路线发送...
	////////////////////////////////////////////////////////////////////////////////////////
	theErr = m_lpUdpLose->SendTo(szPacket, nDataSize);
	// 如果有错误发生，打印出来...
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
	// 打印已发送补包命令...
	blog(LOG_INFO, "%s Multicast Supply Send => Dir: %d, Count: %d, MaxResend: %d", TM_RECV_NAME, DT_TO_SERVER, m_rtp_supply.suSize/sizeof(uint32_t), nCalcMaxResend);
}

void CUDPMultiRecvThread::doParseFrame(bool bIsAudio)
{
	//////////////////////////////////////////////////////////////////////////////////
	// 如果登录还没有收到服务器反馈或播放器为空，都直接返回，继续等待...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_nCmdState <= kCmdSendCreate || m_lpPlaySDL == NULL) {
		//blog(LOG_INFO, "%s Wait For Player => Audio: %d, Video: %d", TM_RECV_NAME, m_audio_circle.size/nPerPackSize, m_video_circle.size/nPerPackSize );
		return;
	}

	// 音视频使用不同的打包对象和变量...
	uint32_t & nMaxPlaySeq = bIsAudio ? m_nAudioMaxPlaySeq : m_nVideoMaxPlaySeq;
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：环形队列至少要有一个数据包存在，否则，在发生丢包时，无法发现，即大号包先到，小号包后到，就会被扔掉...
	// 注意：环形队列在抽取完整音视频数据帧之后，也可能被抽干，所以，必须在 doFillLosePack 中对环形队列为空时做特殊处理...
	// 如果环形队列为空或播放器对象无效，直接返回...
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 准备解析抽包组帧过程中需要的变量和空间对象...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketCurrent[nPerPackSize] = { 0 };
	static char szPacketFront[nPerPackSize] = { 0 };
	rtp_hdr_t * lpFrontHeader = NULL;
	if (cur_circle.size <= nPerPackSize)
		return;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 先找到环形队列中最前面数据包的头指针 => 最小序号...
	circlebuf_peek_front(&cur_circle, szPacketFront, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketFront;
	// 如果最小序号包是需要补充的丢包 => 返回休息等待...
	if (lpFrontHeader->pt == PT_TAG_LOSE)
		return;
	// 如果最小序号包不是音视频数据帧的开始包 => 删除这个数据包，继续找...
	if (lpFrontHeader->pst <= 0) {
		// 更新当前最大播放序列号并保存起来...
		nMaxPlaySeq = lpFrontHeader->seq;
		// 删除这个数据包，返回不休息，继续找...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
		// 修改休息状态 => 已经抽取数据包，不能休息...
		m_bNeedSleep = false;
		// 打印抽帧失败信息 => 没有找到数据帧的开始标记...
		blog(LOG_INFO, "%s Error => Frame start code not find, Seq: %lu, Type: %d, Key: %d, PTS: %lu", TM_RECV_NAME,
			lpFrontHeader->seq, lpFrontHeader->pt, lpFrontHeader->pk, lpFrontHeader->ts);
		return;
	}
	ASSERT(lpFrontHeader->pst);
	// 开始正式从环形队列中抽取音视频数据帧...
	int         pt_type = lpFrontHeader->pt;
	bool        is_key = lpFrontHeader->pk;
	uint32_t    ts_ms = lpFrontHeader->ts;
	uint32_t    min_seq = lpFrontHeader->seq;
	uint32_t    cur_seq = lpFrontHeader->seq;
	rtp_hdr_t * lpCurHeader = lpFrontHeader;
	uint32_t    nConsumeSize = nPerPackSize;
	string      strFrame;
	// 完整数据帧 => 序号连续，以pst开始，以ped结束...
	while (true) {
		// 将数据包的有效载荷保存起来...
		char * lpData = (char*)lpCurHeader + sizeof(rtp_hdr_t);
		strFrame.append(lpData, lpCurHeader->psize);
		// 如果数据包是帧的结束包 => 数据帧组合完毕...
		if (lpCurHeader->ped > 0)
			break;
		// 如果数据包不是帧的结束包 => 继续寻找...
		ASSERT(lpCurHeader->ped <= 0);
		// 累加包序列号，并通过序列号找到包头位置...
		uint32_t nPosition = (++cur_seq - min_seq) * nPerPackSize;
		// 如果包头定位位置超过了环形队列总长度 => 说明已经到达环形队列末尾 => 直接返回，休息等待...
		if (nPosition >= cur_circle.size)
			return;
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
		// 找到指定包头位置的头指针结构体...
		////////////////////////////////////////////////////////////////////////////////////////////////////
		circlebuf_read(&cur_circle, nPosition, szPacketCurrent, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketCurrent;
		// 如果新的数据包不是有效音视频数据包 => 返回等待补包...
		if (lpCurHeader->pt == PT_TAG_LOSE)
			return;
		ASSERT(lpCurHeader->pt != PT_TAG_LOSE);
		// 如果新的数据包不是连续序号包 => 返回等待...
		if (cur_seq != lpCurHeader->seq)
			return;
		ASSERT(cur_seq == lpCurHeader->seq);
		// 如果又发现了帧开始标记 => 清空已解析数据帧 => 这个数据帧不完整，需要丢弃...
		// 同时，需要更新临时存放的数据帧相关信息，重新开始组帧...
		if (lpCurHeader->pst > 0) {
			pt_type = lpCurHeader->pt;
			is_key = lpCurHeader->pk;
			ts_ms = lpCurHeader->ts;
			strFrame.clear();
		}
		// 累加已解析的数据包总长度...
		nConsumeSize += nPerPackSize;
	}
	// 如果没有解析到数据帧 => 打印错误信息...
	if (strFrame.size() <= 0) {
		blog(LOG_INFO, "%s Error => Frame size is Zero, PlaySeq: %lu, Type: %d, Key: %d", TM_RECV_NAME, nMaxPlaySeq, pt_type, is_key);
		return;
	}
	// 注意：环形队列被抽干后，必须在 doFillLosePack 中对环形队列为空时做特殊处理...
	// 如果环形队列被全部抽干 => 也没关系，在收到新包当中对环形队列为空时做了特殊处理...
	/*if( nConsumeSize >= m_circle.size ) {
		blog(LOG_INFO, "%s Error => Circle Empty, PlaySeq: %lu, CurSeq: %lu", TM_RECV_NAME, m_nMaxPlaySeq, cur_seq);
	}*/

	// 注意：已解析的序列号是已经被删除的序列号...
	// 当前已解析的序列号保存为当前最大播放序列号...
	nMaxPlaySeq = cur_seq;
	// 删除已解析完毕的环形队列数据包 => 回收缓冲区...
	circlebuf_pop_front(&cur_circle, NULL, nConsumeSize);
	// 需要对网络缓冲评估延时时间进行线路选择 => 目前只有一条服务器路线...
	int cur_cache_ms = m_server_cache_time_ms;
	// 将解析到的有效数据帧推入播放对象当中...
	if (m_lpPlaySDL != NULL) {
		m_lpPlaySDL->PushPacket(cur_cache_ms, strFrame, pt_type, is_key, ts_ms);
	}
	// 打印已投递的完整数据帧信息...
	//blog(LOG_INFO, "%s Frame => Type: %d, Key: %d, PTS: %lu, Size: %d, PlaySeq: %lu, CircleSize: %d", TM_RECV_NAME,
	//     pt_type, is_key, ts_ms, strFrame.size(), nMaxPlaySeq, cur_circle.size/nPerPackSize );
	// 修改休息状态 => 已经抽取完整音视频数据帧，不能休息...
	m_bNeedSleep = false;
}
///////////////////////////////////////////////////////
// 注意：没有发包，也没有收包，需要进行休息...
///////////////////////////////////////////////////////
void CUDPMultiRecvThread::doSleepTo()
{
	// 如果不能休息，直接返回...
	if (!m_bNeedSleep)
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = os_gettime_ns();
	// 调用系统工具函数，进行sleep休息...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

bool CUDPMultiRecvThread::doVolumeEvent(bool bIsVolPlus)
{
	if (m_lpPlaySDL == NULL) return false;
	return m_lpPlaySDL->doVolumeEvent(bIsVolPlus);
}