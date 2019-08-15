
#include "student-app.h"
#include "pull-thread.h"
#include "BitWritter.h"
#include "ReadSPS.h"

#include "myRTSPClient.h"
#include "window-view-camera.hpp"

CDataThread::CDataThread(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
  , m_bFinished(false)
{
	ASSERT(m_lpViewCamera != NULL);

	m_audio_rate_index = 0;
	m_audio_channel_num = 0;

	m_nVideoHeight = 0;
	m_nVideoWidth = 0;
	m_nVideoFPS = 25;

	m_strPPS.clear();
	m_strSPS.clear();
	m_strAVCHeader.clear();
	m_strAACHeader.clear();
}

CDataThread::~CDataThread()
{
}

// 注意：这里使用接口直接调用，否则，可能会由于异步而造成数据帧丢失...
void CDataThread::ReadyToRecvFrame()
{
	m_lpViewCamera->doReadyToRecvFrame();
}

// 注意：这里使用接口直接调用，否则，无法传递参数...
void CDataThread::PushFrame(FMS_FRAME & inFrame)
{
	m_lpViewCamera->doPushFrame(inFrame);
}

void CDataThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// 获取 width 和 height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// 先保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
}

void CDataThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// 首先解析并存储传递过来的参数...
	if (inAudioRate == 48000)
		m_audio_rate_index = 0x03;
	else if (inAudioRate == 44100)
		m_audio_rate_index = 0x04;
	else if (inAudioRate == 32000)
		m_audio_rate_index = 0x05;
	else if (inAudioRate == 24000)
		m_audio_rate_index = 0x06;
	else if (inAudioRate == 22050)
		m_audio_rate_index = 0x07;
	else if (inAudioRate == 16000)
		m_audio_rate_index = 0x08;
	else if (inAudioRate == 12000)
		m_audio_rate_index = 0x09;
	else if (inAudioRate == 11025)
		m_audio_rate_index = 0x0a;
	else if (inAudioRate == 8000)
		m_audio_rate_index = 0x0b;

	m_audio_channel_num = inAudioChannel;

	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

CRtspThread::CRtspThread(CViewCamera * lpViewCamera)
  : CDataThread(lpViewCamera)
{
	m_env_ = NULL;
	m_nChannelID = 0;
	m_scheduler_ = NULL;
	m_rtspClient_ = NULL;
	m_rtspEventLoopWatchVariable = 0;
}

CRtspThread::~CRtspThread()
{
	// 设置rtsp循环退出标志...
	m_rtspEventLoopWatchVariable = 1;

	blog(LOG_INFO, "== [~CRtspThread Thread] - Exit Start ==");

	// 停止线程...
	this->StopAndWaitForThread();

	blog(LOG_INFO, "== [~CRtspThread Thread] - Exit End ==");
}

int rfind(const char*source, const char* match)
{
	for (int i = strlen(source) - strlen(match) - 1; i >= 0; i--) {
		if (source[i] == match[0] && strncmp(source + i, match, strlen(match)) == 0)
			return i;
	}
	return -1;
}

bool CRtspThread::InitThread()
{
	if (m_lpViewCamera == NULL)
		return false;
	// 获取相关通道的配置...
	GM_MapData theMapData;
	int nDBCameraID = m_lpViewCamera->GetDBCameraID();
	App()->GetCamera(nDBCameraID, theMapData);
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
	// 保存后续需要的参数内容...
	m_strRtspUrl = strStreamUrl;
	// 查找最后一个反斜杠的位置...
	const char * lpMatch = "/";
	const char * lpSource = m_strRtspUrl.c_str();
	int nPosChannel = rfind(lpSource, lpMatch);
	if (nPosChannel > 0) {
		m_nChannelID = atoi(lpSource + nPosChannel + 1);
	}
	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this);
	// 2017.07.21 - by jackey => 有些服务器必须先发OPTIONS...
	// 发起第一次rtsp握手 => 先发起 OPTIONS 命令...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS);
	//启动rtsp检测线程...
	this->Start();
	return true;
}

void CRtspThread::Entry()
{
	// 进行任务循环检测，修改 g_rtspEventLoopWatchVariable 可以让任务退出...
	ASSERT(m_env_ != NULL && m_rtspClient_ != NULL);
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// 设置数据结束标志...
	m_bFinished = true;

	// 任务退出之后，再释放rtsp相关资源...
	// 只能在这里调用 shutdownStream() 其它地方不要调用...
	if (m_rtspClient_ != NULL) {
		m_rtspClient_->shutdownStream();
		m_rtspClient_ = NULL;
	}

	// 释放任务对象...
	if (m_scheduler_ != NULL) {
		delete m_scheduler_;
		m_scheduler_ = NULL;
	}

	// 释放环境变量...
	if (m_env_ != NULL) {
		m_env_->reclaim();
		m_env_ = NULL;
	}
}