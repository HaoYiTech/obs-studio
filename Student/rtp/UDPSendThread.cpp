
#include "student-app.h"
#include "pull-thread.h"
#include "UDPSocket.h"
#include "SocketUtils.h"
#include "UDPSendThread.h"

CUDPSendThread::CUDPSendThread(CDataThread * lpDataThread, int nDBRoomID, int nDBCameraID)
  : m_lpDataThread(lpDataThread)
  , m_total_output_bytes(0)
  , m_audio_output_bytes(0)
  , m_video_output_bytes(0)
  , m_total_output_kbps(0)
  , m_audio_output_kbps(0)
  , m_video_output_kbps(0)
  , m_video_input_bytes(0)
  , m_audio_input_bytes(0)
  , m_audio_input_kbps(0)
  , m_video_input_kbps(0)
  , m_next_create_ns(-1)
  , m_next_header_ns(-1)
  , m_next_detect_ns(-1)
  , m_start_time_ns(0)
  , m_total_time_ms(0)
  , m_bNeedDelete(false)
  , m_bNeedSleep(false)
  , m_lpUDPSocket(NULL)
  , m_HostServerPort(0)
  , m_HostServerAddr(0)
  , m_nAudioCurPackSeq(0)
  , m_nAudioCurSendSeq(0)
  , m_nVideoCurPackSeq(0)
  , m_nVideoCurSendSeq(0)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
  , m_p2p_rtt_var_ms(-1)
  , m_p2p_rtt_ms(-1)
{
	ASSERT(m_lpDataThread != NULL);
	// 初始化发包路线 => 服务器方向...
	m_dt_to_dir = DT_TO_SERVER;
	// 初始化命令状态...
	m_nCmdState = kCmdSendCreate;
	// 初始化rtp序列头结构体...
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// 初始化音视频环形队列，预分配空间...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	circlebuf_reserve(&m_audio_circle, DEF_CIRCLE_SIZE);
	circlebuf_reserve(&m_video_circle, DEF_CIRCLE_SIZE);
	// 设置终端类型和结构体类型 => m_rtp_ready => 等待网络填充...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_header.tm = TM_TAG_STUDENT;
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_header.id = ID_TAG_PUSHER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_header.pt = PT_TAG_HEADER;
	// 填充房间号和直播通道号...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = nDBCameraID;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = nDBCameraID;
	// 填充与远程关联的TCP套接字 => 从全局管理器中获取...
	m_rtp_create.tcpSock = App()->GetRtpTCPSockFD();
	// 初始化环形队列互斥保护对象...
	pthread_mutex_init_value(&m_Mutex);
}

CUDPSendThread::~CUDPSendThread()
{
	blog(LOG_INFO, "%s == [~CUDPSendThread Thread] - Exit Start ==", TM_SEND_NAME);
	// 未知状态，阻止继续塞包...
	m_nCmdState = kCmdUnkownState;
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 关闭UDPSocket对象...
	this->CloseSocket();
	// 释放音视频环形队列空间...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
	// 释放环形队列互斥保护对象...
	pthread_mutex_destroy(&m_Mutex);
	blog(LOG_INFO, "%s == [~CUDPSendThread Thread] - Exit End ==", TM_SEND_NAME);
}

void CUDPSendThread::CloseSocket()
{
	if( m_lpUDPSocket != NULL ) {
		m_lpUDPSocket->Close();
		delete m_lpUDPSocket;
		m_lpUDPSocket = NULL;
	}
}

BOOL CUDPSendThread::InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// 设置视频标志...
	m_rtp_header.hasVideo = true;
	// 保存传递过来的参数信息...
	m_rtp_header.fpsNum = nFPS;
	m_rtp_header.picWidth = nWidth;
	m_rtp_header.picHeight = nHeight;
	m_rtp_header.spsSize = inSPS.size();
	m_rtp_header.ppsSize = inPPS.size();
	m_strSPS = inSPS; m_strPPS = inPPS;
	// 打印已初始化视频信息...
	blog(LOG_INFO, "%s InitVideo OK", TM_SEND_NAME);
	// 线程一定要确认音视频都准备好之后才能启动...
	ASSERT( this->GetThreadHandle() == NULL );
	return true;
}

BOOL CUDPSendThread::InitAudio(int nRateIndex, int nChannelNum)
{
	// 设置音频标志...
	m_rtp_header.hasAudio = true;
	// 保存采样率索引和声道...
	m_rtp_header.rateIndex = nRateIndex;
	m_rtp_header.channelNum = nChannelNum;
	// 打印已初始化音频信息...
	blog(LOG_INFO, "%s InitAudio OK", TM_SEND_NAME);
	// 线程一定要确认音视频都准备好之后才能启动...
	ASSERT( this->GetThreadHandle() == NULL );
	return true;
}

BOOL CUDPSendThread::ParseAVHeader(int nAudioRateIndex, int nAudioChannelNum)
{
	// 如果推流管理器无效，返回失败...
	if (m_lpDataThread == NULL)
		return false;
	// 如果既没有音频也没有视频，返回失败...
	string strAACHeader = m_lpDataThread->GetAACHeader();
	string strAVCHeader = m_lpDataThread->GetAVCHeader();
	if (strAACHeader.size() <= 0 && strAVCHeader.size() <= 0) {
		blog(LOG_INFO, "%s ParseAVHeader error => No Audio and No Video!", TM_SEND_NAME);
		return false;
	}
	// 获取视频相关的格式头信息...
	int nVideoFPS = m_lpDataThread->GetVideoFPS();
	int nVideoWidth = m_lpDataThread->GetVideoWidth();
	int nVideoHeight = m_lpDataThread->GetVideoHeight();
	string strSPS = m_lpDataThread->GetVideoSPS();
	string strPPS = m_lpDataThread->GetVideoPPS();
	// 如果有视频格式头信息，对视频进行初始化...
	if (strAVCHeader.size() > 0) {
		this->InitVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS);
	}
	// 获取音频相关的格式头信息 => 不直接使用摄像头参数...
	int nRateIndex = nAudioRateIndex;
	int nChannelNum = nAudioChannelNum;
	// 如果有音频格式头信息，对音频进行初始化...
	if (strAACHeader.size() > 0) {
		this->InitAudio(nRateIndex, nChannelNum);
	}
	return true;
}

BOOL CUDPSendThread::InitThread(string & strUdpAddr, int nUdpPort, int nAudioRateIndex, int nAudioChannelNum)
{
	// 判断输入参数是否有效...
	if (strUdpAddr.size() <= 0 || nUdpPort <= 0) {
		blog(LOG_INFO, "Invalidate udp-addr or udp-port!");
		return false;
	}
	// 从CPushThread当中解析音视频格式头，解析失败，直接返回...
	if (!this->ParseAVHeader(nAudioRateIndex, nAudioChannelNum))
		return false;
	// 首先，关闭socket...
	this->CloseSocket();
	// 再新建socket...
	ASSERT( m_lpUDPSocket == NULL );
	m_lpUDPSocket = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：Open默认已经设定为异步模式...
	///////////////////////////////////////////////////////
	// 建立UDP,发送音视频数据,接收指令...
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUDPSocket->Open();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return false;
	}
	// 设置重复使用端口...
	m_lpUDPSocket->ReuseAddr();
	// 设置发送和接收缓冲区...
	m_lpUDPSocket->SetSocketSendBufSize(128 * 1024);
	m_lpUDPSocket->SetSocketRecvBufSize(128 * 1024);
	// 设置TTL网络穿越数值...
	m_lpUDPSocket->SetTtl(32);
	// 获取服务器地址信息 => 假设输入信息就是一个IPV4域名...
	const char * lpszAddr = strUdpAddr.c_str();
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		lpszAddr = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
	}
	// 保存服务器地址，简化SendTo参数......
	m_lpUDPSocket->SetRemoteAddr(lpszAddr, nUdpPort);
	// 服务器地址和端口转换成host格式，保存起来...
	m_HostServerStr = lpszAddr;
	m_HostServerPort = nUdpPort;
	m_HostServerAddr = ntohl(inet_addr(lpszAddr));
	// 启动组播接收线程...
	this->Start();
	// 返回执行结果...
	return true;
}

#ifdef DEBUG_FRAME
static void DoSaveSendFile(uint32_t inPTS, int inType, bool bIsKeyFrame, string & strFrame)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/send.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PTS: %lu, Type: %d, Key: %d, Size: %d\n", inPTS, inType, bIsKeyFrame, strFrame.size());
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fwrite(strFrame.c_str(), 1, strFrame.size(), pFile);
	fclose(pFile);
}

static void DoSaveSendSeq(uint32_t inPSeq, int inPSize, bool inPST, bool inPED, uint32_t inPTS)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/send_seq.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PSeq: %lu, PSize: %d, PST: %d, PED: %d, PTS: %lu\n", inPSeq, inPSize, inPST, inPED, inPTS);
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fclose(pFile);
}
#endif // DEBUG_FRAME

BOOL CUDPSendThread::PushFrame(FMS_FRAME & inFrame)
{
	// 判断线程是否已经退出...
	if( this->IsStopRequested() ) {
		blog(LOG_INFO, "%s Error => Send Thread has been stoped", TM_SEND_NAME);
		return false;
	}
	// 如果数据帧的长度为0，打印错误，直接返回...
	if( inFrame.strData.size() <= 0 ) {
		blog(LOG_INFO, "%s Error => Input Frame Size is Zero", TM_SEND_NAME);
		return false;
	}
	/*/////////////////////////////////////////////////////////////////////
	// 注意：只有当接收端准备好之后，才能开始推流操作...
	/////////////////////////////////////////////////////////////////////
	// 如果接收端没有准备好，直接返回 => 收到准备继续包，状态为打包状态...
	if( m_nCmdState != kCmdSendAVPack || m_rtp_ready.tm != TM_TAG_TEACHER || m_rtp_ready.recvPort <= 0 ) {
		blog(LOG_INFO,"[Student-Pusher] Discard for Error State => PTS: %lu, Type: %d, Size: %d", inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.strData.size());
		return false;
	}
	// 如果有视频，第一帧必须是视频关键帧...
	if( m_rtp_header.hasVideo && m_nVideoCurPackSeq <= 0 ) {
		if( inFrame.typeFlvTag != PT_TAG_VIDEO || !inFrame.is_keyframe ) {
			blog(LOG_INFO,"[Student-Pusher] Discard for First Video Frame => PTS: %lu, Type: %d, Size: %d", inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.strData.size());
			return false;
		}
		ASSERT( inFrame.typeFlvTag == PT_TAG_VIDEO && inFrame.is_keyframe );
		blog(LOG_INFO,"[Student-Pusher] StartPTS: %lu, Type: %d, Size: %d", inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.strData.size());
	}*/

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：也可以不限定状态，一开始就进行数据帧的打包，但需要在发包时判断接入状态...
	// 并且，需要在接入失败之后，或 一定时间之后，需要自动清理环形队列，避免数据堆积...
	///////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 只要推流端接入成功之后就可以打包操作了，不要管观看端是否接入...
	// 接入成功标志：成功收到服务器反馈的收到序列头消息通知...
	// 因此，观看端在成功接入，开始接收数据的时候，自己设定自己的0点时刻；
	// 观看端的0点时刻：当收到服务器反馈的序列头命令之后，就可以设定0点时刻，马上就收到音视频数据；
	// 准备就绪的命令作用：是为了获取穿透地址，便于P2P交互，不能做为收发包依据，更不能做为控制条件；
	///////////////////////////////////////////////////////////////////////////////////////////////////
	if( m_nCmdState == kCmdUnkownState || m_nCmdState <= kCmdSendHeader ) {
		blog(LOG_INFO, "%s State Error => PTS: %lu, Type: %d, Size: %d", TM_SEND_NAME, inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.strData.size());
		return false;
	}
	// 打印所有的音视频数据帧...
	//log_trace("[Student-Pusher] Frame => PTS: %lu, Type: %d, Key: %d, Size: %d", inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.strData.size());

	// 保存输入音视频字节总数，用于计算音视频输入码率...
	m_audio_input_bytes += ((inFrame.typeFlvTag == PT_TAG_AUDIO) ? inFrame.strData.size() : 0);
	m_video_input_bytes += ((inFrame.typeFlvTag == PT_TAG_VIDEO) ? inFrame.strData.size() : 0);

#ifdef DEBUG_FRAME
	DoSaveSendFile(inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.strData);
#endif // DEBUG_FRAME

	// 注意：严重拥塞直接中断推流，不进行丢包处理...
	// 如果有音频，且多次发生网络拥塞，全部丢掉视频...
	/*if( m_rtp_header.hasAudio && m_bIsJamAudio && inFrame.typeFlvTag == PT_TAG_VIDEO ) {
		blog(LOG_INFO, "%s Jam => OnlyAudio: %d, Video Drop, PTS: %lu, Size: %d", TM_SEND_NAME, m_bIsJamAudio, inFrame.dwSendTime, inFrame.strData.size());
		return true;
	}
	// 如果发生网络拥塞，视频只发送关键帧...
	if( m_bIsJamFlag && inFrame.typeFlvTag == PT_TAG_VIDEO && !inFrame.is_keyframe ) {
		blog(LOG_INFO, "%s Jam => OnlyAudio: %d, Video Drop, PTS: %lu, Size: %d", TM_SEND_NAME, m_bIsJamAudio, inFrame.dwSendTime, inFrame.strData.size());
		return true;
	}*/

	// 对环形队列相关资源进行互斥保护...
	pthread_mutex_lock(&m_Mutex);

	// 音视频使用不同的打包对象和变量...
	uint32_t & nCurPackSeq = (inFrame.typeFlvTag == PT_TAG_AUDIO) ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
	circlebuf & cur_circle = (inFrame.typeFlvTag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;

	// 构造RTP包头结构体...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_STUDENT;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = inFrame.typeFlvTag;
	rtpHeader.pk  = inFrame.is_keyframe;
	rtpHeader.ts  = inFrame.dwSendTime;
	// 计算需要分片的个数 => 需要注意累加最后一个分片...
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = inFrame.strData.size();
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	const char * lpszFramePtr = inFrame.strData.c_str();
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	int nEndSize = nFrameSize - (nSliceNum - 1) * DEF_MTU_SIZE;
	// 进行循环压入环形队列当中...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++nCurPackSeq; // 累加打包序列号...
		rtpHeader.pst = ((i == 0) ? true : false); // 是否是第一个分片...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // 是否是最后一个分片...
		rtpHeader.psize = rtpHeader.ped ? nEndSize : DEF_MTU_SIZE; // 如果是最后一个分片，取计算值(不能取余数，如果是MTU整数倍会出错)，否则，取MTU值...
		ASSERT( rtpHeader.psize > 0 && rtpHeader.psize <= DEF_MTU_SIZE );
		// 计算填充数据长度...
		int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
		// 计算分片包的数据头指针...
		const char * lpszSlicePtr = lpszFramePtr + i * DEF_MTU_SIZE;
		// 加入环形队列当中 => rtp_hdr_t + slice + Zero => 12 + 800 => 812
		circlebuf_push_back(&cur_circle, &rtpHeader, sizeof(rtpHeader));
		circlebuf_push_back(&cur_circle, lpszSlicePtr, rtpHeader.psize);
		// 加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// 打印调试信息...
		//blog(LOG_INFO, "%s Seq: %lu, Type: %d, Key: %d, Size: %d, TS: %lu", TM_SEND_NAME,
		//		rtpHeader.seq, rtpHeader.pt, rtpHeader.pk, rtpHeader.psize, rtpHeader.ts);
#ifdef DEBUG_FRAME
		DoSaveSendSeq(rtpHeader.seq, rtpHeader.psize, rtpHeader.pst, rtpHeader.ped, rtpHeader.ts);
#endif // DEBUG_FRAME
	}
	// 对环形队列相关资源互斥保护结束...
	pthread_mutex_unlock(&m_Mutex);
	return true;
}

void CUDPSendThread::doCalcAVBitRate()
{
	// 设定码率的检测刻度值 => 越小越精确，可以判断瞬时码率...
	int rate_tick_ms = 1000;
	// 计算持续线程启动到现在总的毫秒数，如果不到1秒钟，直接返回...
	int64_t cur_total_ms = (os_gettime_ns() - m_start_time_ns)/1000000;
	if((cur_total_ms - m_total_time_ms) < rate_tick_ms )
		return;
	// 保存总的持续时间 => 毫秒数...
	m_total_time_ms = cur_total_ms;
	// 根据音视频当前输入输出的总字节数，计算输入输出平均码率...
	m_audio_input_kbps = (m_audio_input_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	m_video_input_kbps = (m_video_input_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_audio_output_kbps = (m_audio_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_video_output_kbps = (m_video_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_total_output_kbps = (m_total_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	m_audio_output_kbps = (m_audio_output_bytes * 8) / 1024; m_audio_output_bytes = 0;
	m_video_output_kbps = (m_video_output_bytes * 8) / 1024; m_video_output_bytes = 0;
	m_total_output_kbps = (m_total_output_bytes * 8) / 1024; m_total_output_bytes = 0;
	// 打印计算获得的音视频输入输出平均码流值...
	//blog(LOG_INFO,"%s AVBitRate =>  audio_input: %d kbps,  video_input: %d kbps", TM_SEND_NAME, m_audio_input_kbps, m_video_input_kbps);
	//blog(LOG_INFO,"%s AVBitRate => audio_output: %d kbps, video_output: %d kbps, total_output: %d kbps", TM_SEND_NAME, m_audio_output_kbps, m_video_output_kbps, m_total_output_kbps);
}

void CUDPSendThread::Entry()
{
	// 码率计算计时起点...
	m_start_time_ns = os_gettime_ns();
	// 开始进行线程循环操作...
	while( !this->IsStopRequested() ) {
		// 计算音视频输入输出平均码流...
		this->doCalcAVBitRate();
		// 设置休息标志 => 只要有发包或收包就不能休息...
		m_bNeedSleep = true;
		// 发送观看端需要的音频丢失数据包...
		this->doSendLosePacket(true);
		// 发送观看端需要的视频丢失数据包...
		this->doSendLosePacket(false);
		// 发送创建房间和直播通道命令包...
		this->doSendCreateCmd();
		// 发送序列头命令包...
		this->doSendHeaderCmd();
		// 发送探测命令包...
		this->doSendDetectCmd();
		// 发送一个封装好的音频RTP数据包...
		this->doSendPacket(true);
		// 发送一个封装好的视频RTP数据包...
		this->doSendPacket(false);
		// 接收一个到达的服务器反馈包...
		this->doRecvPacket();
		// 等待发送或接收下一个数据包...
		this->doSleepTo();
	}
	// 只发送一次删除命令包...
	this->doSendDeleteCmd();
}

void CUDPSendThread::doSendDeleteCmd()
{
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// 套接字有效，直接发送删除命令...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	// 累加总的输出字节数，便于计算输出平均码流...
	m_total_output_bytes += sizeof(m_rtp_delete);
	// 打印已发送删除命令包...
	blog(LOG_INFO, "%s Send Delete RoomID: %lu, LiveID: %d", TM_SEND_NAME, m_rtp_delete.roomID, m_rtp_delete.liveID);
}

void CUDPSendThread::doSendCreateCmd()
{
	// 如果命令状态不是创建命令，不发送命令，直接返回...
	if( m_nCmdState != kCmdSendCreate )
		return;
	// 每隔100毫秒发送创建命令包 => 必须转换成有符号...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// 首先，发送一个创建房间命令...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 累加总的输出字节数，便于计算输出平均码流...
	m_total_output_bytes += sizeof(m_rtp_create);
	// 打印已发送创建命令包 => 第一个包有可能没有发送出去，也返回正常...
	blog(LOG_INFO, "%s Send Create RoomID: %lu, LiveID: %d", TM_SEND_NAME, m_rtp_create.roomID, m_rtp_create.liveID);
	// 计算下次发送创建命令的时间戳...
	m_next_create_ns = os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendHeaderCmd()
{
	// 如果命令状态不是序列头命令，不发送命令，直接返回...
	if( m_nCmdState != kCmdSendHeader )
		return;
	// 每隔100毫秒发送序列头命令包 => 必须转换成有符号...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_header_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_header_ns );
	// 然后，发送序列头命令包...
	string strSeqHeader;
	strSeqHeader.append((char*)&m_rtp_header, sizeof(m_rtp_header));
	// 加入SPS数据区内容...
	if( m_strSPS.size() > 0 ) {
		strSeqHeader.append(m_strSPS);
	}
	// 加入PPS数据区内容...
	if( m_strPPS.size() > 0 ) {
		strSeqHeader.append(m_strPPS);
	}
	// 调用套接字接口，直接发送RTP数据包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 累加总的输出字节数，便于计算输出平均码流...
	m_total_output_bytes += strSeqHeader.size();
	// 打印已发送序列头命令包...
	blog(LOG_INFO, "%s Send Header SPS: %lu, PPS: %d", TM_SEND_NAME, m_strSPS.size(), m_strPPS.size());
	// 计算下次发送创建命令的时间戳...
	m_next_header_ns = os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
{
	// 每隔1秒发送一个探测命令包 => 必须转换成有符号...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// 第一个探测包延时1/3秒发送 => 避免第一个探测包先到达，引发服务器发送重建命令...
	if( m_next_detect_ns < 0 ) { 
		m_next_detect_ns = cur_time_ns + period_ns / 3;
	}
	// 如果发包时间还没到，直接返回...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// 通过服务器中转的探测 => 将探测起点时间转换成毫秒，累加探测计数器...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtDir  = DT_TO_SERVER;
	m_rtp_detect.dtNum += 1;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这种靠丢数据来解决网络拥塞的方法不可取，网络已经拥塞，无法将结果同步到观看端...
	// 注意：没有发生拥塞时，音视频的拥塞点都设置成0，观看端也容易判断...
	// 计算音视频的拥塞点 => 用视频做为判断依据 => 超过多个GOP之后就认为拥塞...
	//this->doCalcAVJamSeq(m_rtp_detect.maxAConSeq, m_rtp_detect.maxVConSeq);
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 采用了新的拥塞处理 => 判断缓存时间，发生严重拥塞，中断推流，等待讲师端自动触发再次推流...
	this->doCalcAVJamFlag();

	// 调用接口发送探测命令包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 累加总的输出字节数，便于计算输出平均码流...
	m_total_output_bytes += sizeof(m_rtp_detect);
	// 通过P2P进行的探测 => 如果观看端的映射地址有效...
	if( m_rtp_ready.recvAddr > 0 && m_rtp_ready.recvPort > 0 ) {
		m_rtp_detect.dtDir = DT_TO_P2P;
		m_rtp_detect.tsSrc = (uint32_t)(cur_time_ns / 1000000);
		theErr = m_lpUDPSocket->SendTo( m_rtp_ready.recvAddr, m_rtp_ready.recvPort, 
										(void*)&m_rtp_detect, sizeof(m_rtp_detect) );
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
		m_total_output_bytes += sizeof(m_rtp_detect);
	}
	// 打印已发送探测命令包...
	//blog(LOG_INFO,"%s Send Detect dtNum: %d", TM_SEND_NAME, m_rtp_detect.dtNum);
	// 计算下次发送探测命令的时间戳...
	m_next_detect_ns = os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doCalcAVJamFlag()
{
	// 视频环形队列为空，没有拥塞，直接返回...
	if( m_video_circle.size <= 0 )
		return;
	// 对环形队列相关资源进行互斥保护...
	pthread_mutex_lock(&m_Mutex);
	// 遍历环形队列，缓存了5秒数据，认为发生了拥塞，设置只能灌输视频关键帧标志...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf & cur_circle = m_video_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_ts = 0;
	uint32_t    max_ts = 0;
	// 读取第一个数据包的内容，获取最小时间戳...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	min_ts = lpCurHeader->ts;
	// 读取第大的数据包的内容，获取最大时间戳...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_ts = lpCurHeader->ts;
	// 对环形队列相关资源互斥保护结束...
	pthread_mutex_unlock(&m_Mutex);
	// 计算环形队列当中总的缓存时间...
	uint32_t cur_buf_ms = max_ts - min_ts;
	// 只要视频总缓存时间超过一定毫秒数，判断网络严重拥塞，立即中断推流操作...
	if( cur_buf_ms >= MAX_SEND_JAM_MS) {
		blog(LOG_INFO, "%s Jam => Stop Push, Buffered %lu ms video data.", TM_SEND_NAME, cur_buf_ms);
		m_bNeedDelete = true;
		return;
	}
	// 打印网络拥塞情况 => 就是视频缓存的拥塞情况...
	//blog(LOG_INFO, "%s Jam => Count: %d, Flag: %d, CurBufMS: %lu, Circle: %d", TM_SEND_NAME, m_nJamCount, m_bIsJamFlag, cur_buf_ms, cur_circle.size/nPerPackSize);

	/*// 如果总缓存时间低于1秒，设置网络恢复标志...
	if( cur_buf_ms <= 1000 ) {
		m_bIsJamFlag = false;
	}
	// 如果总缓存时间超过4秒，需要判断是否有用户接入...
	if( cur_buf_ms >= 4000 ) {
		// 如果观看端还没有接入，直接中断推流操作...
		if( m_nCmdState != kCmdSendAVPack ) {
			blog(LOG_INFO, "%s Jam => Stop Push, After %lu ms no user login", TM_SEND_NAME, cur_buf_ms);
			m_bNeedDelete = true;
			return;
		}
		// 设置网络拥塞标志...
		m_bIsJamFlag = true;
		++m_nJamCount;
	}
	// 如果已经累计发生网络拥堵超过5次，只能发送音频...
	if( m_nJamCount >= 5 ) {
		m_bIsJamFlag = true;
		m_bIsJamAudio = true;
	}*/
}

/*void CUDPSendThread::doCalcAVJamSeq(uint32_t & outAudioSeq, uint32_t & outVideoSeq)
{
	// 设置默认的拥塞序号为0...
	outAudioSeq = outVideoSeq = 0;
	// 视频环形队列为空，没有拥塞，直接返回...
	if( m_video_circle.size <= 0 )
		return;
	// 遍历环形队列，如果有两个关键帧存在，删除第一个关键帧之前的所有数据包...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf & cur_circle = m_video_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_seq = 0;
	uint32_t    nPosition = 0;
	int         nGopCount = 0;
	// 读取第一个数据包的内容，获取最小序号包，保存以供参考...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpCurHeader->seq;
	// 循环遍历视频环形队列...
	while( true ) {
		// 如果是关键帧的起始帧 => 关键帧计数增加...
		if( lpCurHeader->pk > 0 && lpCurHeader->pst > 0 ) {
			// 如果超过5个关键帧 => 发生拥塞， 删除当前包之前的所有数据包...
			if( ++nGopCount >= 5 ) {
				// 删除当前包之前的所有数据包...
				circlebuf_pop_front(&cur_circle, NULL, nPosition);
				// 打印视频拥塞信息 => 当前位置，已发送数据包 => 两者之差就是观看端的有效补包空间...
				log_trace("%s Jam => Video MinSeq: %lu, JamSeq: %lu, SendSeq: %lu, Cirlce: %d", TM_SEND_NAME, min_seq, lpCurHeader->seq, m_nVideoCurSendSeq, cur_circle.size/nPerPackSize);
				// 直接将发包位置调整当前包，从当前包开始发包...
				m_nVideoCurSendSeq = lpCurHeader->seq;
				// 拥塞序号包设置为当前序号包...
				outVideoSeq = lpCurHeader->seq;
				// 删除音频相关时间的数据包，返回拥塞音频序号包...
				outAudioSeq = this->doEarseAudioByTime(lpCurHeader->ts);
				return;
			}
		}
		// 如果不是关键帧的开始包，并且没有超过预定关键帧数，累加读取位置...
		nPosition += nPerPackSize;
		// 已经读取到达环形队列尾部，直接跳出循环...
		if( nPosition >= cur_circle.size )
			break;
		// 读取没有越界的环形队列...
		circlebuf_read(&cur_circle, nPosition, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	}
}
//
// 删除指定的拥塞时间，并返回拥塞点...
uint32_t CUDPSendThread::doEarseAudioByTime(uint32_t inTimeStamp)
{
	// 视频已经发生拥塞，音频默认拥塞点为：当前已发送序号包...
	uint32_t nAudioJamSeq = m_nAudioCurSendSeq;
	// 音频环形队列为空，返回默认的已发送序号包...
	if( m_audio_circle.size <= 0 )
		return nAudioJamSeq;
	// 遍历环形队列，删除所有时间戳小于输入时间戳的数据包，保持音视频时间戳同步...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf & cur_circle = m_audio_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    nPosition = 0;
	uint32_t    min_seq = 0;
	// 读取第一个音频数据包的内容，保存最小序号，以供参考...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	min_seq = lpCurHeader->seq;
	// 循环遍历音频环形队列...
	while( true ) {
		// 如果当前包的时间戳大于或等于输入时间戳，删除这个序号包之前的所有数据包...
		if( lpCurHeader->ts >= inTimeStamp )
			break;
		// 当前包的时间戳小于输入时间戳...
		ASSERT( lpCurHeader->ts < inTimeStamp );
		// 已经读取到环形队列尾部，保留一个数据包...
		if( (nPosition + nPerPackSize) >= cur_circle.size )
			break;
		// 没有到达尾部，读取下一个数据包...
		nPosition += nPerPackSize;
		// 读取没有越界的环形队列，获取有效对象...
		circlebuf_read(&cur_circle, nPosition, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	}
	// 注意：当期包是拷贝出来的，缓存始终有效...
	// 删除当前包之前的所有数据包...
	circlebuf_pop_front(&cur_circle, NULL, nPosition);
	// 打印音频拥塞信息 => 当前位置，已发送数据包 => 两者之差就是观看端的有效补包空间...
	log_trace("%s Jam => Audio MinSeq: %lu, JamSeq: %lu, SendSeq: %lu, Circle: %d", TM_SEND_NAME, min_seq, lpCurHeader->seq, m_nAudioCurSendSeq, cur_circle.size/nPerPackSize);
	// 直接将发包位置调整当前包，从当前包开始发包...
	m_nAudioCurSendSeq = lpCurHeader->seq;
	// 拥塞序号设置为当前序号包...
	nAudioJamSeq = lpCurHeader->seq;
	// 返回最终获取到的音频拥塞点...
	return nAudioJamSeq;
}*/

void CUDPSendThread::doSendLosePacket(bool bIsAudio)
{
	if( m_lpUDPSocket == NULL )
		return;
	// 对环形队列相关资源进行互斥保护...
	pthread_mutex_lock(&m_Mutex);
	do {
		// 根据数据包类型，找到丢包集合、环形队列...
		GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
		circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
		// 丢包集合队列为空，直接返回...
		if( theMapLose.size() <= 0 )
			break;
		// 拿出一个丢包记录，无论是否发送成功，都要删除这个丢包记录...
		// 如果观看端，没有收到这个数据包，会再次发起补包命令...
		GM_MapLose::iterator itorItem = theMapLose.begin();
		rtp_lose_t rtpLose = itorItem->second;
		theMapLose.erase(itorItem);
		// 如果环形队列为空，直接返回...
		if( cur_circle.size <= 0 )
			break;
		// 先找到环形队列中最前面数据包的头指针 => 最小序号...
		GM_Error    theErr = GM_NoErr;
		rtp_hdr_t * lpFrontHeader = NULL;
		rtp_hdr_t * lpSendHeader = NULL;
		int nSendPos = 0, nSendSize = 0;
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
		// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
		/////////////////////////////////////////////////////////////////////////////////////////////////
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		static char szPacketBuffer[nPerPackSize] = {0};
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果要补充的数据包序号比最小序号还要小 => 没有找到，直接返回...
		if( rtpLose.lose_seq < lpFrontHeader->seq ) {
			blog(LOG_INFO, "%s Supply Error => lose: %lu, min: %lu, Type: %d", TM_SEND_NAME, rtpLose.lose_seq, lpFrontHeader->seq, rtpLose.lose_type);
			break;
		}
		ASSERT( rtpLose.lose_seq >= lpFrontHeader->seq );
		// 注意：环形队列当中的序列号一定是连续的...
		// 两者之差就是要发送数据包的头指针位置...
		nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
		// 如果补包位置大于或等于环形队列长度 => 补包越界...
		if( nSendPos >= cur_circle.size ) {
			blog(LOG_INFO, "%s Supply Error => Position Excessed", TM_SEND_NAME);
			break;
		}
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
		// 获取将要发送数据包的包头位置和有效数据长度...
		////////////////////////////////////////////////////////////////////////////////////////////////////
		memset(szPacketBuffer, 0, nPerPackSize);
		circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
		lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果找到的序号位置不对 => 缓存里面的所有包都是有效包，因为是数据源头...
		if( lpSendHeader->seq != rtpLose.lose_seq ) {
			blog(LOG_INFO, "%s Supply Error => Seq: %lu, Find: %lu, Type: %d", TM_SEND_NAME, rtpLose.lose_seq, lpSendHeader->seq, rtpLose.lose_type);
			break;
		}
		// 获取有效的数据区长度 => 包头 + 数据...
		nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
		////////////////////////////////////////////////////////////////////////////////////////
		// 调用套接字接口，直接发送RTP数据包 => 根据发包线路进行有选择的路线发送...
		// 目前只有两条线路可供选择 => TO_SERVER or TO_P2P...
		////////////////////////////////////////////////////////////////////////////////////////
		if((m_dt_to_dir == DT_TO_P2P) && (m_rtp_ready.recvAddr > 0) && (m_rtp_ready.recvPort > 0)) {
			theErr = m_lpUDPSocket->SendTo(m_rtp_ready.recvAddr, m_rtp_ready.recvPort, (void*)lpSendHeader, nSendSize);
		} else {
			ASSERT( m_dt_to_dir == DT_TO_SERVER );
			theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
		}
		// 如果有错误发生，打印出来...
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
		// 累加总的输出字节数，便于计算输出平均码流...
		m_total_output_bytes += nSendSize;
		// 打印已经发送补包信息...
		//blog(LOG_INFO, "%s Supply Send => Dir: %d, Seq: %lu, TS: %lu, Slice: %d, Type: %d", TM_SEND_NAME, m_dt_to_dir, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->psize, lpSendHeader->pt);
		// 修改休息状态 => 已经有发包，不能休息...
		m_bNeedSleep = false;
	} while (false);
	// 对环形队列相关资源互斥保护结束...
	pthread_mutex_unlock(&m_Mutex);
}

void CUDPSendThread::doSendPacket(bool bIsAudio)
{
	// 对环形队列相关资源进行互斥保护...
	pthread_mutex_lock(&m_Mutex);
	do {
		// 根据数据包类型，找到打包序号、发包序号、环形队列...
		uint32_t  & nCurPackSeq = bIsAudio ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
		uint32_t  & nCurSendSeq = bIsAudio ? m_nAudioCurSendSeq : m_nVideoCurSendSeq;
		circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
		// 如果环形队列没有可发送数据，直接返回...
		if( cur_circle.size <= 0 || m_lpUDPSocket == NULL )
			break;
		// 如果要发送序列号比打包序列号还要大 => 没有数据包可以发送...
		if( nCurSendSeq > nCurPackSeq )
			break;
		// 取出最前面的RTP数据包，但不从环形队列中移除 => 目的是给接收端补包用...
		GM_Error    theErr = GM_NoErr;
		rtp_hdr_t * lpFrontHeader = NULL;
		rtp_hdr_t * lpSendHeader = NULL;
		int nSendPos = 0, nSendSize = 0;
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
		// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
		/////////////////////////////////////////////////////////////////////////////////////////////////
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		static char szPacketBuffer[nPerPackSize] = {0};
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		// 计算环形队列中最前面数据包的头指针 => 最小序号...
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// 第一次发包 或 发包序号太小 => 使用最前面包的序列号...
		if((nCurSendSeq <= 0) || (nCurSendSeq < lpFrontHeader->seq)) {
			nCurSendSeq = lpFrontHeader->seq;
		}
		/////////////////////////////////////////////////////////////////////////////////
		// 环形队列最小序号 => min_id => lpFrontHeader->seq
		// 环形队列最大序号 => max_id => nCurPackSeq
		/////////////////////////////////////////////////////////////////////////////////
		// 将要发送的数据包序号不能小于最前面包的序列号...
		ASSERT( nCurSendSeq >= lpFrontHeader->seq );
		ASSERT( nCurSendSeq <= nCurPackSeq );
		// 两者之差就是要发送数据包的头指针位置...
		nSendPos = (nCurSendSeq - lpFrontHeader->seq) * nPerPackSize;
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
		// 获取将要发送数据包的包头位置和有效数据长度...
		////////////////////////////////////////////////////////////////////////////////////////////////////
		memset(szPacketBuffer, 0, nPerPackSize);
		circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
		lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果要发送的数据位置越界或无效，直接返回...
		if( lpSendHeader == NULL || lpSendHeader->seq <= 0 )
			break;
		// 获取有效的数据区长度 => 包头 + 数据...
		nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
		////////////////////////////////////////////////////////////////////////////////////////
		// 调用套接字接口，直接发送RTP数据包 => 根据发包线路进行有选择的路线发送...
		// 目前只有两条线路可供选择 => TO_SERVER or TO_P2P...
		////////////////////////////////////////////////////////////////////////////////////////
		if((m_dt_to_dir == DT_TO_P2P) && (m_rtp_ready.recvAddr > 0) && (m_rtp_ready.recvPort > 0)) {
			theErr = m_lpUDPSocket->SendTo(m_rtp_ready.recvAddr, m_rtp_ready.recvPort, (void*)lpSendHeader, nSendSize);
		} else {
			ASSERT( m_dt_to_dir == DT_TO_SERVER );
			theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
		}
		// 如果有错误发生，打印出来...
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;

		// 累加总体、音视频输出总字节数...
		m_total_output_bytes += nSendSize;
		m_audio_output_bytes += (bIsAudio ? nSendSize : 0);
		m_video_output_bytes += (bIsAudio ? 0 : nSendSize);

		/////////////////////////////////////////////////////////////////////////////////
		// 实验：随机丢包...
		/////////////////////////////////////////////////////////////////////////////////
		/*if( nCurSendSeq % 3 != 2 ) {
			// 调用套接字接口，直接发送RTP数据包...
			theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
			(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
		}*/
		/////////////////////////////////////////////////////////////////////////////////

		// 成功发送数据包 => 累加发送序列号...
		++nCurSendSeq;
		// 修改休息状态 => 已经有发包，不能休息...
		m_bNeedSleep = false;

		// 打印调试信息 => 刚刚发送的数据包...
		//int nZeroSize = DEF_MTU_SIZE - lpSendHeader->psize;
		//blog(LOG_INFO, "%s Type: %d, Seq: %lu, TS: %lu, pst: %d, ped: %d, Slice: %d, Zero: %d", TM_SEND_NAME, lpSendHeader->pt, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pst, lpSendHeader->ped, lpSendHeader->psize, nZeroSize);
	} while (false);
	// 对环形队列相关资源互斥保护结束...
	pthread_mutex_unlock(&m_Mutex);
}

void CUDPSendThread::doRecvPacket()
{
	if( m_lpUDPSocket == NULL )
		return;
	GM_Error theErr = GM_NoErr;
	UInt32   outRecvLen = 0;
	UInt32   outRemoteAddr = 0;
	UInt16   outRemotePort = 0;
	UInt32   inBufLen = MAX_BUFF_LEN;
	char     ioBuffer[MAX_BUFF_LEN] = {0};
	// 调用接口从网络层获取数据包 => 这里是异步套接字，会立即返回 => 不管错误...
	theErr = m_lpUDPSocket->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// 注意：这里不用打印错误信息，没有收到数据就立即返回...
	if( theErr != GM_NoErr || outRecvLen <= 0 )
		return;
	// 修改休息状态 => 已经成功收包，不能休息...
	m_bNeedSleep = false;
 
	// 对环形队列相关资源进行互斥保护...
	pthread_mutex_lock(&m_Mutex);

	// 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;

	// 对收到命令包进行类型分发...
	switch( ptTag )
	{
	case PT_TAG_CREATE:  this->doProcServerCreate(ioBuffer, outRecvLen); break;
	case PT_TAG_HEADER:  this->doProcServerHeader(ioBuffer, outRecvLen); break;
	case PT_TAG_READY:   this->doProcServerReady(ioBuffer, outRecvLen);  break;

	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_SUPPLY:  this->doTagSupplyProcess(ioBuffer, outRecvLen); break;
	}

	// 对环形队列相关资源互斥保护结束...
	pthread_mutex_unlock(&m_Mutex);
}

void CUDPSendThread::doProcServerCreate(char * lpBuffer, int inRecvLen)
{
	// 通过 rtp_hdr_t 做为载体发送过来的...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// 获取数据包结构体...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// 判断数据包的有效性 => 必须是服务器反馈的 Create 命令...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_CREATE )
		return;
	// 修改命令状态 => 开始发送序列头...
	m_nCmdState = kCmdSendHeader;
	// 打印收到服务器反馈的创建命令包...
	blog(LOG_INFO, "%s Recv Create from Server", TM_SEND_NAME);
}

void CUDPSendThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// 通过 rtp_hdr_t 做为载体发送过来的...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// 获取数据包结构体...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// 判断数据包的有效性 => 必须是服务器反馈的序列头命令...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_HEADER )
		return;
	// 修改命令状态 => 开始接收观看端准备就绪命令...
	m_nCmdState = kCmdWaitReady;
	// 打印收到服务器反馈的序列头命令包...
	blog(LOG_INFO, "%s Recv Header from Server", TM_SEND_NAME);
}

void CUDPSendThread::doProcServerReady(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_ready_t) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果不是 老师观看端 发出的准备就绪包，直接返回...
	if( tmTag != TM_TAG_TEACHER || idTag != ID_TAG_LOOKER )
		return;
	// 修改命令状态 => 可以进行音视频打包发送数据了...
	m_nCmdState = kCmdSendAVPack;
	// 保存老师观看端发送的准备就绪数据包内容...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
	// 打印收到准备就绪命令包 => 将地址转换成字符串...
	string strAddr = SocketUtils::ConvertAddrToString(m_rtp_ready.recvAddr);
	blog(LOG_INFO, "%s Recv Ready from %s:%d", TM_SEND_NAME, strAddr.c_str(), m_rtp_ready.recvPort);
	// 立即反馈给老师观看者 => 准备就绪包已经收到，不要再发了...
	rtp_ready_t rtpReady = {0};
	rtpReady.tm = TM_TAG_STUDENT;
	rtpReady.id = ID_TAG_PUSHER;
	rtpReady.pt = PT_TAG_READY;
	// 调用套接字接口，直接发送RTP数据包...
	GM_Error theErr = m_lpUDPSocket->SendTo(&rtpReady, sizeof(rtpReady));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 累加总的输出字节数，便于计算输出平均码流...
	m_total_output_bytes += sizeof(rtpReady);
	// 打印发送准备就绪回复命令包...
	blog(LOG_INFO, "%s Send Ready command for reply", TM_SEND_NAME);
}

void CUDPSendThread::doTagSupplyProcess(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_supply_t) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果不是 老师观看端 发出的准备就绪包，直接返回...
	if( tmTag != TM_TAG_TEACHER || idTag != ID_TAG_LOOKER )
		return;
	// 获取补包命令包内容...
	rtp_supply_t rtpSupply = {0};
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, lpBuffer, nHeadSize);
	if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inRecvLen) )
		return;
	// 根据数据包类型，找到丢包集合...
	GM_MapLose & theMapLose = (rtpSupply.suType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// 获取需要补包的序列号，加入到补包队列当中...
	char * lpDataPtr = lpBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while( nDataSize > 0 ) {
		uint32_t   nLoseSeq = 0;
		rtp_lose_t rtpLose = {0};
		// 获取补包序列号...
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// 如果序列号已经存在，增加补包次数，不存在，增加新记录...
		if( theMapLose.find(nLoseSeq) != theMapLose.end() ) {
			rtp_lose_t & theFind = theMapLose[nLoseSeq];
			theFind.lose_type = rtpSupply.suType;
			theFind.lose_seq = nLoseSeq;
			++theFind.resend_count;
		} else {
			rtpLose.lose_seq = nLoseSeq;
			rtpLose.lose_type = rtpSupply.suType;
			rtpLose.resend_time = (uint32_t)(os_gettime_ns() / 1000000);
			theMapLose[rtpLose.lose_seq] = rtpLose;
		}
		// 移动数据区指针位置...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
	}
	// 打印已收到补包命令...
	//blog(LOG_INFO, "%s Supply Recv => Count: %d, Type: %d", TM_SEND_NAME, rtpSupply.suSize / sizeof(int), rtpSupply.suType);
}

void CUDPSendThread::doProcMaxConSeq(bool bIsAudio, uint32_t inMaxConSeq)
{
	// 根据数据包类型，找到环形队列、最大播放序号、当前拥塞点...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	uint32_t & nCurSendSeq = bIsAudio ? m_nAudioCurSendSeq : m_nVideoCurSendSeq;
	uint32_t & nCurPackSeq = bIsAudio ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
	// 如果输入的最大连续包号无效或环形队列为空，直接返回...
	if( inMaxConSeq <= 0 || cur_circle.size <= 0 )
		return;
	// 先找到环形队列中最前面数据包的头指针 => 最小序号...
	rtp_hdr_t * lpFrontHeader = NULL;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// 如果要删除的数据包序号比最小序号还要小 => 数据已经删除了，直接返回...
	if( inMaxConSeq < lpFrontHeader->seq )
		return;
	// 注意：当前已发送包号是一个超前包号，是指下一个要发送的包号...
	// 观看端收到的最大连续包号相等或大于当前已发送包号 => 直接返回...
	if( inMaxConSeq >= nCurSendSeq )
		return;
	// 注意：环形队列当中的序列号一定是连续的...
	// 注意：观看端收到的最大连续包号一定比当前已发送包号小...
	// 两者之差加1就是要删除的数据长度 => 要包含最大连续包本身的删除...
	uint32_t nPopSize = (inMaxConSeq - lpFrontHeader->seq + 1) * nPerPackSize;
	circlebuf_pop_front(&cur_circle, NULL, nPopSize);
	// 注意：环形队列当中的数据块大小是连续的，是一样大的...
	// 打印环形队列删除结果，计算环形队列剩余的数据包个数...
	uint32_t nRemainCount = cur_circle.size / nPerPackSize;
	//blog(LOG_INFO, "%s Detect Erase Success => %s, MaxConSeq: %lu, MinSeq: %lu, CurSendSeq: %lu, CurPackSeq: %lu, Circle: %lu", TM_SEND_NAME,
	//			bIsAudio ? "Audio" : "Video", inMaxConSeq, lpFrontHeader->seq, nCurSendSeq, nCurPackSeq, nRemainCount );
}

void CUDPSendThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果是 老师观看者 发出的探测包，将收到的探测数据包原样返回给服务器端...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
		rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
		// 先将收到探测数据包原样返回给发送者 => 目前只有两条线路可供选择 => TO_SERVER or TO_P2P...
		if((lpDetect->dtDir == DT_TO_P2P) && (m_rtp_ready.recvAddr > 0) && (m_rtp_ready.recvPort > 0)) {
			theErr = m_lpUDPSocket->SendTo(m_rtp_ready.recvAddr, m_rtp_ready.recvPort, lpBuffer, inRecvLen);
		} else {
			// 注意：有可能没有收到观看端映射地址，但收到了观看端P2P探测包，这时，需要通过服务器中转探测包...
			theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		}
		// 累加总的输出字节数，便于计算输出平均码流...
		m_total_output_bytes += inRecvLen;
		// 先处理音频最大连续序号包...
		this->doProcMaxConSeq(true, lpDetect->maxAConSeq);
		// 再处理视频最大连续序号包...
		this->doProcMaxConSeq(false, lpDetect->maxVConSeq);
		return;
	}
	// 如果是 学生推流端 自己发出的探测包，计算网络延时...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		// 获取收到的探测数据包...
		rtp_detect_t rtpDetect = {0};
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// 当前时间转换成毫秒，计算网络延时 => 当前时间 - 探测时间...
		uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
		int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
		// 处理来自服务器方向的探测结果...
		if( rtpDetect.dtDir == DT_TO_SERVER ) {
			// 防止网络突发延迟增大, 借鉴 TCP 的 RTT 遗忘衰减的算法...
			if( m_server_rtt_ms < 0 ) { m_server_rtt_ms = keep_rtt; }
			else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
			// 计算网络抖动的时间差值 => RTT的修正值...
			if( m_server_rtt_var_ms < 0 ) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
			else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
			// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
			//blog(LOG_INFO, "%s Recv Detect => Dir: %d, dtNum: %d, rtt: %d ms, rtt_var: %d ms", TM_SEND_NAME, rtpDetect.dtDir, rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms);
		}
		// 处理来自P2P方向的探测结果...
		if( rtpDetect.dtDir == DT_TO_P2P ) {
			// 防止网络突发延迟增大, 借鉴 TCP 的 RTT 遗忘衰减的算法...
			if( m_p2p_rtt_ms < 0 ) { m_p2p_rtt_ms = keep_rtt; }
			else { m_p2p_rtt_ms = (7 * m_p2p_rtt_ms + keep_rtt) / 8; }
			// 计算网络抖动的时间差值 => RTT的修正值...
			if( m_p2p_rtt_var_ms < 0 ) { m_p2p_rtt_var_ms = abs(m_p2p_rtt_ms - keep_rtt); }
			else { m_p2p_rtt_var_ms = (m_p2p_rtt_var_ms * 3 + abs(m_p2p_rtt_ms - keep_rtt)) / 4; }
			// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
			//blog(LOG_INFO, "%s Recv Detect => Dir: %d, dtNum: %d, rtt: %d ms, rtt_var: %d ms", TM_SEND_NAME, rtpDetect.dtDir, rtpDetect.dtNum, m_p2p_rtt_ms, m_p2p_rtt_var_ms);
		}
		//////////////////////////////////////////////////////////////////////////////
		// 对发包线路进行选择 => 选择已联通的最小rtt进行发送正常包和发送补包...
		//////////////////////////////////////////////////////////////////////////////
		// 如果P2P线路有效，并且更快，设定为P2P线路，否则，设定为服务器线路...
		if((m_p2p_rtt_ms >= 0) && (m_p2p_rtt_ms < m_server_rtt_ms)) {
			m_dt_to_dir = DT_TO_P2P;
		} else {
			m_dt_to_dir = DT_TO_SERVER;
		}
	}
}
///////////////////////////////////////////////////////
// 注意：没有发包，也没有收包，需要进行休息...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
{
	// 如果不能休息，直接返回...
	if( !m_bNeedSleep )
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = os_gettime_ns();
	// 调用系统工具函数，进行sleep休息...
	os_sleepto_ns(cur_time_ns + delta_ns);
}