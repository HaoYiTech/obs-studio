
#include <obs-avc.h>
#include "UDPSocket.h"
#include "SocketUtils.h"
#include "UDPSendThread.h"

#define MILLISECOND_DEN   1000
static int32_t get_ms_time(struct encoder_packet *packet, int64_t val)
{
	return (int32_t)(val * MILLISECOND_DEN / packet->timebase_den);
}

CUDPSendThread::CUDPSendThread(int nTCPSockFD, int nDBRoomID)
  : m_total_output_bytes(0)
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
  , m_bNeedSleep(false)
  , m_lpUDPSocket(NULL)
  , m_lpObsOutput(NULL)
  , m_HostServerPort(0)
  , m_HostServerAddr(0)
  , m_nAudioCurPackSeq(0)
  , m_nAudioCurSendSeq(0)
  , m_nVideoCurPackSeq(0)
  , m_nVideoCurSendSeq(0)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
  , m_start_dts_ms(-1)
{
	// ��ʼ������·�� => ����������...
	m_dt_to_dir = DT_TO_SERVER;
	// ��ʼ������״̬...
	m_nCmdState = kCmdSendCreate;
	// ��ʼ��rtp����ͷ�ṹ��...
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	// ��ʼ������Ƶ���ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
	circlebuf_reserve(&m_audio_circle, DEF_CIRCLE_SIZE);
	circlebuf_reserve(&m_video_circle, DEF_CIRCLE_SIZE);
	// �����ն����ͺͽṹ������ => m_rtp_ready => �ȴ�������� => ��ʦ���������...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_header.tm = TM_TAG_TEACHER;
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_header.id = ID_TAG_PUSHER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_header.pt = PT_TAG_HEADER;
	// ��䷿��ź�ֱ��ͨ����...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = 0;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = 0;
	// �����Զ�̹�����TCP�׽���...
	m_rtp_create.tcpSock = nTCPSockFD;
}

CUDPSendThread::~CUDPSendThread()
{
	blog(LOG_INFO, "%s == [~CUDPSendThread Thread] - Exit Start ==", TM_SEND_NAME);
	// δ֪״̬����ֹ��������...
	m_nCmdState = kCmdUnkownState;
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// �ر�UDPSocket����...
	this->CloseSocket();
	// �ͷ�����Ƶ���ζ��пռ�...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
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
	// ������Ƶ��־...
	m_rtp_header.hasVideo = true;
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_rtp_header.fpsNum = nFPS;
	m_rtp_header.picWidth = nWidth;
	m_rtp_header.picHeight = nHeight;
	m_rtp_header.spsSize = (uint16_t)inSPS.size();
	m_rtp_header.ppsSize = (uint16_t)inPPS.size();
	m_strSPS = inSPS; m_strPPS = inPPS;
	// ��ӡ�ѳ�ʼ����Ƶ��Ϣ...
	blog(LOG_INFO, "%s InitVideo OK", TM_SEND_NAME);
	// �߳�һ��Ҫȷ������Ƶ��׼����֮���������...
	ASSERT( this->GetThreadHandle() == NULL );
	return true;
}

BOOL CUDPSendThread::InitAudio(int inAudioRate, int inAudioChannel)
{
	int	audio_rate_index = 0;
	// ���Ƚ������洢���ݹ����Ĳ���...
	if (inAudioRate == 48000)
		audio_rate_index = 0x03;
	else if (inAudioRate == 44100)
		audio_rate_index = 0x04;
	else if (inAudioRate == 32000)
		audio_rate_index = 0x05;
	else if (inAudioRate == 24000)
		audio_rate_index = 0x06;
	else if (inAudioRate == 22050)
		audio_rate_index = 0x07;
	else if (inAudioRate == 16000)
		audio_rate_index = 0x08;
	else if (inAudioRate == 12000)
		audio_rate_index = 0x09;
	else if (inAudioRate == 11025)
		audio_rate_index = 0x0a;
	else if (inAudioRate == 8000)
		audio_rate_index = 0x0b;
	// ������Ƶ��־...
	m_rtp_header.hasAudio = true;
	// �������������������...
	m_rtp_header.rateIndex = audio_rate_index;
	m_rtp_header.channelNum = inAudioChannel;
	// ��ӡ�ѳ�ʼ����Ƶ��Ϣ...
	blog(LOG_INFO, "%s InitAudio OK", TM_SEND_NAME);
	// �߳�һ��Ҫȷ������Ƶ��׼����֮���������...
	ASSERT( this->GetThreadHandle() == NULL );
	return true;
}

BOOL CUDPSendThread::ParseAVHeader()
{
	// �ж�obs��������Ƿ���Ч...
	if (m_lpObsOutput == NULL)
		return false;
	// �Ȼ�ȡ��Ƶ��صĸ�ʽͷ��Ϣ...
	obs_encoder_t * lpVEncoder = obs_output_get_video_encoder(m_lpObsOutput);
	video_output_info * lpVideo = (video_output_info*)obs_encoder_video(lpVEncoder);
	uint8_t * lpVHeader = NULL; uint8_t * lpSPS = NULL; uint8_t * lpPPS = NULL;
	size_t v_size = 0; size_t sps_size = 0; size_t pps_size = 0;
	string strSPS, strPPS;
	// ���û�л�ȡ����Ч����Ƶ��ʽͷ����ֱ�ӷ���...
	if (lpVideo == NULL || lpVEncoder == NULL) {
		blog(LOG_INFO, "%s ParseAVHeader Failed.", TM_SEND_NAME);
		return false;
	}
	// �����ȡ��Ƶ��չ��ʽͷ��Ϣʧ�ܣ����ش���...
	if (!obs_encoder_get_extra_data(lpVEncoder, &lpVHeader, &v_size)) {
		blog(LOG_INFO, "%s ParseAVHeader Failed.", TM_SEND_NAME);
		return false;
	}
	// �жϻ�ȡ����Ƶ��չ��ʽͷ��Ϣ�Ƿ���Ч...
	if (lpVHeader == NULL || v_size <= 0) {
		blog(LOG_INFO, "%s ParseAVHeader Failed.", TM_SEND_NAME);
		return false;
	}
	// ������Ƶ��sps��pps��ʽ��Ϣ������ʧ�ܣ�ֱ�ӷ���...
	if (!obs_get_sps_pps(lpVHeader, v_size, &lpSPS, &sps_size, &lpPPS, &pps_size)) {
		blog(LOG_INFO, "%s ParseAVHeader Failed.", TM_SEND_NAME);
		return false;
	}
	// ��SPS��PPS����Ϊstring����...
	strSPS.assign((char*)lpSPS, sps_size);
	strPPS.assign((char*)lpPPS, pps_size);
	// ��ȡ��Ƶ����صĸ�ʽͷ��Ϣ...
	obs_encoder_t * lpAEncoder = obs_output_get_audio_encoder(m_lpObsOutput, 0);
	audio_output_info * lpAudio = (audio_output_info*)obs_encoder_audio(lpAEncoder);
	if( lpAEncoder == NULL || lpAudio == NULL ) {
		blog(LOG_INFO, "%s ParseAVHeader Failed.", TM_SEND_NAME);
		return false;
	}
	// ���ݻ�ȡ������Ƶ��ʽͷ����������Ƶ����ĳ�ʼ������...
	this->InitVideo(strSPS, strPPS, lpVideo->width, lpVideo->height, lpVideo->fps_num);
	this->InitAudio(lpAudio->samples_per_sec, (int)lpAudio->speakers);
	return true;
}

BOOL CUDPSendThread::InitThread(obs_output_t * lpObsOutput, const char * lpUdpAddr, int nUdpPort)
{
	// ���沢����obs����Ƶ��ʽͷ��Ϣ...
	m_lpObsOutput = lpObsOutput;
	// ��obs�н�������Ƶ��ʽͷ������ʧ�ܣ�ֱ�ӷ���...
	if (!this->ParseAVHeader())
		return false;
	// ���ȣ��ر�socket...
	this->CloseSocket();
	// ���½�socket...
	ASSERT( m_lpUDPSocket == NULL );
	m_lpUDPSocket = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺OpenĬ���Ѿ��趨Ϊ�첽ģʽ...
	///////////////////////////////////////////////////////
	// ����UDP,��������Ƶ����,����ָ��...
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUDPSocket->Open();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return false;
	}
	// �����ظ�ʹ�ö˿�...
	m_lpUDPSocket->ReuseAddr();
	// ���÷��ͺͽ��ջ�����...
	m_lpUDPSocket->SetSocketSendBufSize(128 * 1024);
	m_lpUDPSocket->SetSocketRecvBufSize(128 * 1024);
	// ����TTL���紩Խ��ֵ...
	m_lpUDPSocket->SetTtl(32);
	// ��ȡ��������ַ��Ϣ => ����������Ϣ����һ��IPV4����...
	const char * lpszAddr = lpUdpAddr;
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		lpszAddr = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
	}
	// �����������ַ����SendTo����......
	m_lpUDPSocket->SetRemoteAddr(lpszAddr, nUdpPort);
	// ��������ַ�Ͷ˿�ת����host��ʽ����������...
	m_HostServerPort = nUdpPort;
	m_HostServerAddr = ntohl(inet_addr(lpszAddr));
	// �����鲥�����߳�...
	this->Start();
	// ����ִ�н��...
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

BOOL CUDPSendThread::PushFrame(encoder_packet * lpEncPacket)
{
	OSMutexLocker theLock(&m_Mutex);
	// �ж��߳��Ƿ��Ѿ��˳�...
	if( this->IsStopRequested() ) {
		blog(LOG_INFO, "%s Error => Send Thread has been stoped", TM_SEND_NAME);
		return false;
	}
	// �������֡�ĳ���Ϊ0����ӡ����ֱ�ӷ���...
	if( lpEncPacket->size <= 0 ) {
		blog(LOG_INFO, "%s Error => Input Frame Size is Zero", TM_SEND_NAME);
		return false;
	}

	// ���㵱ǰ֡��dtsʱ�䣬�Լ���pts��ʱ��� => ��Ƶ����ţ���Ƶ�ǿ̶�...
	int32_t cur_offset_ms = get_ms_time(lpEncPacket, lpEncPacket->pts - lpEncPacket->dts);
	int32_t cur_time_ms = get_ms_time(lpEncPacket, lpEncPacket->dts);
	// ��¼��һ֡��ʱ��ƫ��ֵ => Ĭ��ֵ�Ǹ���...
	if (m_start_dts_ms < 0) { m_start_dts_ms = cur_time_ms; }
	// ��ǰ֡ʱ���ȥ��һ֡ʱ��ƫ�� => ע������Ϊ���������...
	cur_time_ms -= m_start_dts_ms;
	if (cur_time_ms < 0) { cur_time_ms = 0; }

	// ����ȡ�����ݰ����б���ת�ƣ��������ʹ��...
	int pt_type = ((lpEncPacket->type == OBS_ENCODER_VIDEO) ? PT_TAG_VIDEO : PT_TAG_AUDIO);
	uint32_t dwSendTime = (uint32_t)cur_time_ms;
	bool is_keyframe = lpEncPacket->keyframe;
	uint8_t * pDataPtr = lpEncPacket->data;
	size_t nDataSize = lpEncPacket->size;

	// ��ӡ���е�����Ƶ����֡...
	//blog(LOG_INFO, "%s Frame => PTS: %lu, Offset: %d, Type: %d, Key: %d, Size: %d", TM_SEND_NAME, dwSendTime, cur_offset_ms, pt_type, is_keyframe, nDataSize);

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// ���ӷ������ɹ�֮�󣬲�����ѹ��������ˣ�һ�������ӳɹ�״̬...
	///////////////////////////////////////////////////////////////////////////////////////////////////
	if( m_nCmdState == kCmdUnkownState || m_nCmdState <= kCmdSendHeader ) {
		blog(LOG_INFO, "%s State Error => PTS: %lu, Type: %d, Size: %d", TM_SEND_NAME, dwSendTime, pt_type, nDataSize);
		return false;
	}

	// ������������Ƶ�ֽ����������ڼ�������Ƶ��������...
	m_audio_input_bytes += ((pt_type == PT_TAG_AUDIO) ? nDataSize : 0);
	m_video_input_bytes += ((pt_type == PT_TAG_VIDEO) ? nDataSize : 0);

	// ����Ƶʹ�ò�ͬ�Ĵ������ͱ���...
	uint32_t & nCurPackSeq = (pt_type == PT_TAG_AUDIO) ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
	circlebuf & cur_circle = (pt_type == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;

	// ����RTP��ͷ�ṹ�� => ��ʦ������...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_TEACHER;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = pt_type;
	rtpHeader.pk  = is_keyframe;
	rtpHeader.ts  = dwSendTime;
	// ������Ҫ��Ƭ�ĸ��� => ��Ҫע���ۼ����һ����Ƭ...
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = nDataSize;
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	char * lpszFramePtr = (char*)pDataPtr;
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	int nEndSize = nFrameSize - (nSliceNum - 1) * DEF_MTU_SIZE;
	// ����ѭ��ѹ�뻷�ζ��е���...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++nCurPackSeq; // �ۼӴ�����к�...
		rtpHeader.pst = ((i == 0) ? true : false); // �Ƿ��ǵ�һ����Ƭ...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // �Ƿ������һ����Ƭ...
		rtpHeader.psize = rtpHeader.ped ? nEndSize : DEF_MTU_SIZE; // ��������һ����Ƭ��ȡ����ֵ(����ȡ�����������MTU�����������)������ȡMTUֵ...
		ASSERT( rtpHeader.psize > 0 && rtpHeader.psize <= DEF_MTU_SIZE );
		// ����������ݳ���...
		int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
		// �����Ƭ��������ͷָ��...
		const char * lpszSlicePtr = lpszFramePtr + i * DEF_MTU_SIZE;
		// ���뻷�ζ��е��� => rtp_hdr_t + slice + Zero => 12 + 800 => 812
		circlebuf_push_back(&cur_circle, &rtpHeader, sizeof(rtpHeader));
		circlebuf_push_back(&cur_circle, lpszSlicePtr, rtpHeader.psize);
		// ��������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// ��ӡ������Ϣ...
		//blog(LOG_INFO, "%s Seq: %lu, Type: %d, Key: %d, Size: %d, TS: %lu", TM_SEND_NAME,
		//		rtpHeader.seq, rtpHeader.pt, rtpHeader.pk, rtpHeader.psize, rtpHeader.ts);
	}
	return true;
}

void CUDPSendThread::doCalcAVBitRate()
{
	// �趨���ʵļ��̶�ֵ => ԽСԽ��ȷ�������ж�˲ʱ����...
	int rate_tick_ms = 1000;
	// ��������߳������������ܵĺ��������������1���ӣ�ֱ�ӷ���...
	int64_t cur_total_ms = (os_gettime_ns() - m_start_time_ns)/1000000;
	if((cur_total_ms - m_total_time_ms) < rate_tick_ms )
		return;
	// �����ܵĳ���ʱ�� => ������...
	m_total_time_ms = cur_total_ms;
	// ��������Ƶ��ǰ������������ֽ����������������ƽ������...
	m_audio_input_kbps = (int)(m_audio_input_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	m_video_input_kbps = (int)(m_video_input_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_audio_output_kbps = (m_audio_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_video_output_kbps = (m_video_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	//m_total_output_kbps = (m_total_output_bytes * 8 / (m_total_time_ms / rate_tick_ms)) / 1024;
	m_audio_output_kbps = (int)(m_audio_output_bytes * 8) / 1024; m_audio_output_bytes = 0;
	m_video_output_kbps = (int)(m_video_output_bytes * 8) / 1024; m_video_output_bytes = 0;
	m_total_output_kbps = (int)(m_total_output_bytes * 8) / 1024; m_total_output_bytes = 0;
	// ��ӡ�����õ�����Ƶ�������ƽ������ֵ...
	//blog(LOG_INFO, "%s AVBitRate =>  audio_input: %d kbps,  video_input: %d kbps", TM_SEND_NAME, m_audio_input_kbps, m_video_input_kbps);
	//blog(LOG_INFO, "%s AVBitRate => audio_output: %d kbps, video_output: %d kbps, total_output: %d kbps", TM_SEND_NAME, m_audio_output_kbps, m_video_output_kbps, m_total_output_kbps);
}

void CUDPSendThread::Entry()
{
	// ���ʼ����ʱ���...
	m_start_time_ns = os_gettime_ns();
	// ��ʼ�����߳�ѭ������...
	while( !this->IsStopRequested() ) {
		// ��������Ƶ�������ƽ������...
		this->doCalcAVBitRate();
		// ������Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
		m_bNeedSleep = true;
		// ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
		this->doSendLosePacket(true);
		// ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
		this->doSendLosePacket(false);
		// ���ʹ��������ֱ��ͨ�������...
		this->doSendCreateCmd();
		// ��������ͷ�����...
		this->doSendHeaderCmd();
		// ����̽�������...
		this->doSendDetectCmd();
		// ����һ����װ�õ���ƵRTP���ݰ�...
		this->doSendPacket(true);
		// ����һ����װ�õ���ƵRTP���ݰ�...
		this->doSendPacket(false);
		// ����һ������ķ�����������...
		this->doRecvPacket();
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
	// ֻ����һ��ɾ�������...
	this->doSendDeleteCmd();
}

void CUDPSendThread::doSendDeleteCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// �׽�����Ч��ֱ�ӷ���ɾ������...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += sizeof(m_rtp_delete);
	// ��ӡ�ѷ���ɾ�������...
	blog(LOG_INFO, "%s Send Delete RoomID: %lu", TM_SEND_NAME, m_rtp_delete.roomID);
}

void CUDPSendThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������״̬���Ǵ���������������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendCreate )
		return;
	// ÿ��100���뷢�ʹ�������� => ����ת�����з���...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// ���ȣ�����һ��������������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += sizeof(m_rtp_create);
	// ��ӡ�ѷ��ʹ�������� => ��һ�����п���û�з��ͳ�ȥ��Ҳ��������...
	blog(LOG_INFO, "%s Send Create RoomID: %lu", TM_SEND_NAME, m_rtp_create.roomID);
	// �����´η��ʹ��������ʱ���...
	m_next_create_ns = os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendHeaderCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������״̬��������ͷ������������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendHeader )
		return;
	// ÿ��100���뷢������ͷ����� => ����ת�����з���...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_header_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_header_ns );
	// Ȼ�󣬷�������ͷ�����...
	string strSeqHeader;
	strSeqHeader.append((char*)&m_rtp_header, sizeof(m_rtp_header));
	// ����SPS����������...
	if( m_strSPS.size() > 0 ) {
		strSeqHeader.append(m_strSPS);
	}
	// ����PPS����������...
	if( m_strPPS.size() > 0 ) {
		strSeqHeader.append(m_strPPS);
	}
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += strSeqHeader.size();
	// ��ӡ�ѷ�������ͷ�����...
	blog(LOG_INFO, "%s Send Header SPS: %lu, PPS: %d", TM_SEND_NAME, m_strSPS.size(), m_strPPS.size());
	// �����´η��ʹ��������ʱ���...
	m_next_header_ns = os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ÿ��1�뷢��һ��̽������� => ����ת�����з���...
	int64_t cur_time_ns = os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// ��һ��̽�����ʱ1/3�뷢�� => �����һ��̽����ȵ�����������������ؽ�����...
	if( m_next_detect_ns < 0 ) { 
		m_next_detect_ns = cur_time_ns + period_ns / 3;
	}
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// ͨ����������ת��̽�� => ��̽�����ʱ��ת���ɺ��룬�ۼ�̽�������...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtDir  = DT_TO_SERVER;
	m_rtp_detect.dtNum += 1;

	// ע�⣺��ʦ�����˲����������в������������ŵ������������...
	// ��ˣ������˸���̽�������ж����Ĵ������������ǹ̶�����ķ���...
	// �������µ�ӵ������ => ɾ��ָ������ʱ���֮ǰ������Ƶ���ݰ�...
	//this->doCalcAVJamStatus();

	// ���ýӿڷ���̽������� => ��ʦ������ֻ��һ��������̽�ⷽ��...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += sizeof(m_rtp_detect);
	// ��ӡ�ѷ���̽�������...
	//blog(LOG_INFO, "%s Send Detect dtNum: %d", TM_SEND_NAME, m_rtp_detect.dtNum);
	// �����´η���̽�������ʱ���...
	m_next_detect_ns = os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doCalcAVJamStatus()
{
	// ��Ƶ���ζ���Ϊ�գ�û��ӵ����ֱ�ӷ���...
	if( m_video_circle.size <= 0 )
		return;
	// �������ζ��У�ɾ�����г���n��Ļ������ݰ� => �����Ƿ��ǹؼ�֡����������ֻ��Ϊ����������...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	uint32_t    cur_send_seq = m_nVideoCurSendSeq;
	circlebuf & cur_circle = m_video_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_ts = 0, min_seq = 0;
	uint32_t    max_ts = 0, max_seq = 0;
	// ��ȡ�ڴ�����ݰ������ݣ���ȡ���ʱ���...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	max_ts = lpCurHeader->ts;
	// �������ζ��У��鿴�Ƿ�����Ҫɾ�������ݰ�...
	while ( cur_circle.size > 0 ) {
		// ��ȡ��һ�����ݰ������ݣ���ȡ��Сʱ���...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// �����ǰ���ű�����Ͱ��Ŵ� => ֻ��ɾ���ѷ������ݰ�...
		if (lpCurHeader->seq >= cur_send_seq)
			break;
		// ���㻷�ζ��е����ܵĻ���ʱ��...
		uint32_t cur_buf_ms = max_ts - lpCurHeader->ts;
		// ����ܻ���ʱ�䲻����n�룬�жϲ���...
		if (cur_buf_ms < 5000)
			break;
		ASSERT(cur_buf_ms >= 5000);
		// ����ɾ����ʱ��㣬����Ƶ�ο�...
		min_ts = lpCurHeader->ts;
		min_seq = lpCurHeader->seq;
		// ����ܻ���ʱ�䳬��n�룬ɾ����С���ݰ�������Ѱ��...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// ���û�з���ɾ����ֱ�ӷ���...
	if (min_ts <= 0 || min_seq <= 0 )
		return;
	// ��ӡ����ӵ����� => ������Ƶ�����ӵ�����...
	//blog(LOG_INFO, "%s Video Jam => MinSeq: %lu, MaxSeq: %lu, SendSeq: %lu, Circle: %d", TM_SEND_NAME, min_seq, max_seq, cur_send_seq, cur_circle.size/nPerPackSize);
	// ɾ����Ƶ���ʱ������ݰ� => �������ʱ���֮ǰ���������ݰ�����ɾ��...
	this->doEarseAudioByPTS(min_ts);
}
//
// ɾ����Ƶ���ʱ������ݰ�...
void CUDPSendThread::doEarseAudioByPTS(uint32_t inTimeStamp)
{
	// ��Ƶ���ζ���Ϊ�գ�ֱ�ӷ���...
	if (m_audio_circle.size <= 0)
		return;
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	uint32_t    cur_send_seq = m_nAudioCurSendSeq;
	circlebuf & cur_circle = m_audio_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_seq = 0, max_seq = 0;
	// ��ȡ�ڴ�����ݰ������ݣ���ȡ������к�...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	// ������Ƶ���ζ��У�ɾ������ʱ���С������ʱ��������ݰ�...
	while( cur_circle.size > 0 ) {
		// ��ȡ��һ�����ݰ������ݣ���ȡ��Сʱ���...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// �����ǰ���ű�����Ͱ��Ŵ� => ֻ��ɾ���ѷ������ݰ�...
		if (lpCurHeader->seq >= cur_send_seq)
			break;
		// �����ǰ����ʱ�����������ʱ�����ɾ����ֹ...
		if (lpCurHeader->ts > inTimeStamp)
			break;
		// ɾ����ǰ��С���ݰ�������Ѱ��...
		min_seq = lpCurHeader->seq;
		ASSERT(lpCurHeader->ts <= inTimeStamp);
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// ��ӡ��Ƶӵ����Ϣ => ��ǰλ�ã��ѷ������ݰ� => ����֮����ǹۿ��˵���Ч�����ռ�...
	//blog(LOG_INFO, "%s Audio Jam => MinSeq: %lu, MaxSeq: %lu, SendSeq: %lu, Circle: %d", TM_SEND_NAME, min_seq, max_seq, cur_send_seq, cur_circle.size/nPerPackSize);
}

void CUDPSendThread::doSendLosePacket(bool bIsAudio)
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpUDPSocket == NULL )
		return;
	// �������ݰ����ͣ��ҵ��������ϡ����ζ���...
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	// �������϶���Ϊ�գ�ֱ�ӷ���...
	if( theMapLose.size() <= 0 )
		return;
	// �ó�һ��������¼�������Ƿ��ͳɹ�����Ҫɾ�����������¼...
	// ����ۿ��ˣ�û���յ�������ݰ������ٴη��𲹰�����...
	GM_MapLose::iterator itorItem = theMapLose.begin();
	rtp_lose_t rtpLose = itorItem->second;
	theMapLose.erase(itorItem);
	// ������ζ���Ϊ�գ�ֱ�ӷ���...
	if( cur_circle.size <= 0 )
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	size_t nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// ���Ҫ��������ݰ���ű���С��Ż�ҪС => û���ҵ���ֱ�ӷ���...
	if( rtpLose.lose_seq < lpFrontHeader->seq ) {
		blog(LOG_INFO, "%s Supply Error => lose: %lu, min: %lu, Type: %d", TM_SEND_NAME, rtpLose.lose_seq, lpFrontHeader->seq, rtpLose.lose_type);
		return;
	}
	ASSERT( rtpLose.lose_seq >= lpFrontHeader->seq );
	// ע�⣺���ζ��е��е����к�һ����������...
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
	// �������λ�ô��ڻ���ڻ��ζ��г��� => ����Խ��...
	if( nSendPos >= cur_circle.size ) {
		blog(LOG_INFO, "%s Supply Error => Position Excessed", TM_SEND_NAME);
		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
	// ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// ����ҵ������λ�ò��� => ������������а�������Ч������Ϊ������Դͷ...
	if( lpSendHeader->seq != rtpLose.lose_seq ) {
		blog(LOG_INFO, "%s Supply Error => Seq: %lu, Find: %lu, Type: %d", TM_SEND_NAME, rtpLose.lose_seq, lpSendHeader->seq, rtpLose.lose_type);
		return;
	}
	// ��ȡ��Ч������������ => ��ͷ + ����...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// ע�⣺��ʦ������ֻ����һ������������Ĳ���·��...
	ASSERT(m_dt_to_dir == DT_TO_SERVER);
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	// ����д���������ӡ����...
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += nSendSize;
	// ��ӡ�Ѿ����Ͳ�����Ϣ...
	blog(LOG_INFO, "%s Supply Send => Dir: %d, Seq: %lu, TS: %lu, Slice: %d, Type: %d", TM_SEND_NAME, m_dt_to_dir, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->psize, lpSendHeader->pt);
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendPacket(bool bIsAudio)
{
	OSMutexLocker theLock(&m_Mutex);
	// �������ݰ����ͣ��ҵ������š�������š����ζ���...
	uint32_t  & nCurPackSeq = bIsAudio ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
	uint32_t  & nCurSendSeq = bIsAudio ? m_nAudioCurSendSeq : m_nVideoCurSendSeq;
	circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	// ������ζ���û�пɷ������ݣ�ֱ�ӷ���...
	if( cur_circle.size <= 0 || m_lpUDPSocket == NULL )
		return;
	// ���Ҫ�������кűȴ�����кŻ�Ҫ�� => û�����ݰ����Է���...
	if( nCurSendSeq > nCurPackSeq )
		return;
	// ȡ����ǰ���RTP���ݰ��������ӻ��ζ������Ƴ� => Ŀ���Ǹ����ն˲�����...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	size_t nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	// ���㻷�ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// ��һ�η��� �� �������̫С => ʹ����ǰ��������к�...
	if((nCurSendSeq <= 0) || (nCurSendSeq < lpFrontHeader->seq)) {
		nCurSendSeq = lpFrontHeader->seq;
	}
	/////////////////////////////////////////////////////////////////////////////////
	// ���ζ�����С��� => min_id => lpFrontHeader->seq
	// ���ζ��������� => max_id => nCurPackSeq
	/////////////////////////////////////////////////////////////////////////////////
	// ��Ҫ���͵����ݰ���Ų���С����ǰ��������к�...
	ASSERT( nCurSendSeq >= lpFrontHeader->seq );
	ASSERT( nCurSendSeq <= nCurPackSeq );
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (nCurSendSeq - lpFrontHeader->seq) * nPerPackSize;
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
	// ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// ���Ҫ���͵�����λ��Խ�����Ч��ֱ�ӷ���...
	if( lpSendHeader == NULL || lpSendHeader->seq <= 0 )
		return;
	// ��ȡ��Ч������������ => ��ͷ + ����...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// ע�⣺��ʦ������ֻ����һ������������ķ���·��...
	ASSERT(m_dt_to_dir == DT_TO_SERVER);
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	// ����д���������ӡ����...
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;

	// �ۼ����塢����Ƶ������ֽ���...
	m_total_output_bytes += nSendSize;
	m_audio_output_bytes += (bIsAudio ? nSendSize : 0);
	m_video_output_bytes += (bIsAudio ? 0 : nSendSize);

	/////////////////////////////////////////////////////////////////////////////////
	// ʵ�飺�������...
	/////////////////////////////////////////////////////////////////////////////////
	/*if( nCurSendSeq % 3 != 2 ) {
		// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
		theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}*/
	/////////////////////////////////////////////////////////////////////////////////

	// �ɹ��������ݰ� => �ۼӷ������к�...
	++nCurSendSeq;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;

	// ��ӡ������Ϣ => �ոշ��͵����ݰ�...
	//int nZeroSize = DEF_MTU_SIZE - lpSendHeader->psize;
	//blog(LOG_INFO, "%s Type: %d, Seq: %lu, TS: %lu, pst: %d, ped: %d, Slice: %d, Zero: %d", TM_SEND_NAME, lpSendHeader->pt, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pst, lpSendHeader->ped, lpSendHeader->psize, nZeroSize);
}

void CUDPSendThread::doRecvPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpUDPSocket == NULL )
		return;
	GM_Error theErr = GM_NoErr;
	UInt32   outRecvLen = 0;
	UInt32   outRemoteAddr = 0;
	UInt16   outRemotePort = 0;
	UInt32   inBufLen = MAX_BUFF_LEN;
	char     ioBuffer[MAX_BUFF_LEN] = {0};
	// ���ýӿڴ�������ȡ���ݰ� => �������첽�׽��֣����������� => ���ܴ���...
	theErr = m_lpUDPSocket->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// ע�⣺���ﲻ�ô�ӡ������Ϣ��û���յ����ݾ���������...
	if( theErr != GM_NoErr || outRecvLen <= 0 )
		return;
	// �޸���Ϣ״̬ => �Ѿ��ɹ��հ���������Ϣ...
	m_bNeedSleep = false;
 
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;

	// ���յ�������������ͷַ�...
	switch( ptTag )
	{
	case PT_TAG_CREATE:  this->doProcServerCreate(ioBuffer, outRecvLen); break;
	case PT_TAG_HEADER:  this->doProcServerHeader(ioBuffer, outRecvLen); break;

	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_SUPPLY:  this->doTagSupplyProcess(ioBuffer, outRecvLen); break;
	}
}

void CUDPSendThread::doProcServerCreate(char * lpBuffer, int inRecvLen)
{
	// ͨ�� rtp_hdr_t ��Ϊ���巢�͹�����...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ��ȡ���ݰ��ṹ��...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// �ж����ݰ�����Ч�� => �����Ƿ����������� Create ����...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_CREATE )
		return;
	// �޸�����״̬ => ��ʼ��������ͷ...
	m_nCmdState = kCmdSendHeader;
	// ��ӡ�յ������������Ĵ��������...
	blog(LOG_INFO, "%s Recv Create from Server", TM_SEND_NAME);
}

void CUDPSendThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// ͨ�� rtp_hdr_t ��Ϊ���巢�͹�����...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ��ȡ���ݰ��ṹ��...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// �ж����ݰ�����Ч�� => �����Ƿ���������������ͷ����...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_HEADER )
		return;
	// �޸�����״̬ => ��ʼ���չۿ���׼����������...
	m_nCmdState = kCmdSendAVPack;
	// ��ӡ�յ�����������������ͷ�����...
	blog(LOG_INFO, "%s Recv Header from Server", TM_SEND_NAME);
	// ����֪ͨ�ϲ���Բ�׽����Ƶ�����ˣ�֮��Ż����ѹ������֡...
	obs_output_begin_data_capture(m_lpObsOutput, 0);
}

/*void CUDPSendThread::doProcServerReady(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_ready_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ������� ѧ���ۿ��� ������׼����������ֱ�ӷ���...
	if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_LOOKER )
		return;
	///////////////////////////////////////////////////////
	// ע�⣺��Ҫ������ѧ���˵�׼����������...
	///////////////////////////////////////////////////////
	// �޸�����״̬ => ���Խ�������Ƶ�������������...
	m_nCmdState = kCmdSendAVPack;
	// ������ʦ�ۿ��˷��͵�׼���������ݰ�����...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
	// ��ӡ�յ�׼����������� => ����ַת�����ַ���...
	string strAddr = SocketUtils::ConvertAddrToString(m_rtp_ready.recvAddr);
	blog(LOG_INFO, "%s Recv Ready from %s:%d", TM_SEND_NAME, strAddr.c_str(), m_rtp_ready.recvPort);
	// ������������ʦ�ۿ��� => ׼���������Ѿ��յ�����Ҫ�ٷ���...
	rtp_ready_t rtpReady = {0};
	rtpReady.tm = TM_TAG_STUDENT;
	rtpReady.id = ID_TAG_PUSHER;
	rtpReady.pt = PT_TAG_READY;
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo(&rtpReady, sizeof(rtpReady));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
	m_total_output_bytes += sizeof(rtpReady);
	// ��ӡ����׼�������ظ������...
	blog(LOG_INFO, "%s Send Ready command for reply", TM_SEND_NAME);
}*/

void CUDPSendThread::doTagSupplyProcess(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_supply_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ע�⣺ֻ���� �������� �����Ĳ�������...
	if( tmTag != TM_TAG_SERVER || idTag != ID_TAG_SERVER )
		return;
	// ��ȡ�������������...
	rtp_supply_t rtpSupply = {0};
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, lpBuffer, nHeadSize);
	if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inRecvLen) )
		return;
	// �������ݰ����ͣ��ҵ���������...
	GM_MapLose & theMapLose = (rtpSupply.suType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// ��ȡ��Ҫ���������кţ����뵽�������е���...
	char * lpDataPtr = lpBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while( nDataSize > 0 ) {
		uint32_t   nLoseSeq = 0;
		rtp_lose_t rtpLose = {0};
		// ��ȡ�������к�...
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// ������к��Ѿ����ڣ����Ӳ��������������ڣ������¼�¼...
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
		// �ƶ�������ָ��λ��...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
	}
	// ��ӡ���յ���������...
	blog(LOG_INFO, "%s Supply Recv => Count: %d, Type: %d", TM_SEND_NAME, rtpSupply.suSize / sizeof(int), rtpSupply.suType);
}

void CUDPSendThread::doProcMaxConSeq(bool bIsAudio, uint32_t inMaxConSeq)
{
	// �������ݰ����ͣ��ҵ����ζ��С���󲥷���š���ǰӵ����...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	uint32_t & nCurSendSeq = bIsAudio ? m_nAudioCurSendSeq : m_nVideoCurSendSeq;
	uint32_t & nCurPackSeq = bIsAudio ? m_nAudioCurPackSeq : m_nVideoCurPackSeq;
	// ���������������������Ч���ζ���Ϊ�գ�ֱ�ӷ���...
	if( inMaxConSeq <= 0 || cur_circle.size <= 0 )
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	rtp_hdr_t * lpFrontHeader = NULL;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// ���Ҫɾ�������ݰ���ű���С��Ż�ҪС => �����Ѿ�ɾ���ˣ�ֱ�ӷ���...
	if( inMaxConSeq < lpFrontHeader->seq )
		return;
	// ע�⣺��ǰ�ѷ��Ͱ�����һ����ǰ���ţ���ָ��һ��Ҫ���͵İ���...
	// �ۿ����յ����������������Ȼ���ڵ�ǰ�ѷ��Ͱ��� => ֱ�ӷ���...
	if( inMaxConSeq >= nCurSendSeq )
		return;
	// ע�⣺���ζ��е��е����к�һ����������...
	// ע�⣺�ۿ����յ��������������һ���ȵ�ǰ�ѷ��Ͱ���С...
	// ����֮���1����Ҫɾ�������ݳ��� => Ҫ������������������ɾ��...
	uint32_t nPopSize = (inMaxConSeq - lpFrontHeader->seq + 1) * nPerPackSize;
	circlebuf_pop_front(&cur_circle, NULL, nPopSize);
	// ע�⣺���ζ��е��е����ݿ��С�������ģ���һ�����...
	// ��ӡ���ζ���ɾ����������㻷�ζ���ʣ������ݰ�����...
	uint32_t nRemainCount = cur_circle.size / nPerPackSize;
	blog(LOG_INFO, "%s Detect Erase Success => %s, MaxConSeq: %lu, MinSeq: %lu, CurSendSeq: %lu, CurPackSeq: %lu, Circle: %lu", 
		 TM_SEND_NAME, bIsAudio ? "Audio" : "Video", inMaxConSeq, lpFrontHeader->seq, nCurSendSeq, nCurPackSeq, nRemainCount );
}

void CUDPSendThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ����� ������ ������̽��������յ���̽�����ݰ�ԭ�����ظ���������...
	if (tmTag == TM_TAG_SERVER && idTag == ID_TAG_SERVER) {
		rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		// �ۼ��ܵ�����ֽ��������ڼ������ƽ������...
		m_total_output_bytes += inRecvLen;
		// �ȴ�����Ƶ���������Ű�...
		this->doProcMaxConSeq(true, lpDetect->maxAConSeq);
		// �ٴ�����Ƶ���������Ű�...
		this->doProcMaxConSeq(false, lpDetect->maxVConSeq);
		return;
	}
	// ����� ��ʦ������ �Լ�������̽���������������ʱ...
	if (tmTag == TM_TAG_TEACHER && idTag == ID_TAG_PUSHER) {
		// ��ȡ�յ���̽�����ݰ�...
		rtp_detect_t rtpDetect = { 0 };
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// ֻ��һ��̽��·�� => ������̽�ⷽ��...
		ASSERT(rtpDetect.dtDir == DT_TO_SERVER);
		// ��ǰʱ��ת���ɺ��룬����������ʱ => ��ǰʱ�� - ̽��ʱ��...
		uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
		int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
		// ��ֹ����ͻ���ӳ�����, ��� TCP �� RTT ����˥�����㷨...
		if (m_server_rtt_ms < 0) { m_server_rtt_ms = keep_rtt; }
		else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
		// �������綶����ʱ���ֵ => RTT������ֵ...
		if (m_server_rtt_var_ms < 0) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
		else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		//blog(LOG_INFO, "%s Recv Detect => Dir: %d, dtNum: %d, rtt: %d ms, rtt_var: %d ms", TM_SEND_NAME, rtpDetect.dtDir, rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms);
	}
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
{
	// ���������Ϣ��ֱ�ӷ���...
	if( !m_bNeedSleep )
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = os_gettime_ns();
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	os_sleepto_ns(cur_time_ns + delta_ns);
}