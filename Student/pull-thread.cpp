
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

// ע�⣺����ʹ�ýӿ�ֱ�ӵ��ã����򣬿��ܻ������첽���������֡��ʧ...
void CDataThread::ReadyToRecvFrame()
{
	m_lpViewCamera->doReadyToRecvFrame();
}

// ע�⣺����ʹ�ýӿ�ֱ�ӵ��ã������޷����ݲ���...
void CDataThread::PushFrame(FMS_FRAME & inFrame)
{
	m_lpViewCamera->doPushFrame(inFrame);
}

void CDataThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// ��ȡ width �� height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// �ȱ��� SPS �� PPS ��ʽͷ��Ϣ..
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

	// ����AVC����ͷ��Ϣ...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
}

void CDataThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// ���Ƚ������洢���ݹ����Ĳ���...
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

	// ����AAC����ͷ��Ϣ...
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
	// ����rtspѭ���˳���־...
	m_rtspEventLoopWatchVariable = 1;

	blog(LOG_INFO, "== [~CRtspThread Thread] - Exit Start ==");

	// ֹͣ�߳�...
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
	// ��ȡ���ͨ��������...
	GM_MapData theMapData;
	int nDBCameraID = m_lpViewCamera->GetDBCameraID();
	App()->GetCamera(nDBCameraID, theMapData);
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
	// ���������Ҫ�Ĳ�������...
	m_strRtspUrl = strStreamUrl;
	// �������һ����б�ܵ�λ��...
	const char * lpMatch = "/";
	const char * lpSource = m_strRtspUrl.c_str();
	int nPosChannel = rfind(lpSource, lpMatch);
	if (nPosChannel > 0) {
		m_nChannelID = atoi(lpSource + nPosChannel + 1);
	}
	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this);
	// 2017.07.21 - by jackey => ��Щ�����������ȷ�OPTIONS...
	// �����һ��rtsp���� => �ȷ��� OPTIONS ����...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS);
	//����rtsp����߳�...
	this->Start();
	return true;
}

void CRtspThread::Entry()
{
	// ��������ѭ����⣬�޸� g_rtspEventLoopWatchVariable �����������˳�...
	ASSERT(m_env_ != NULL && m_rtspClient_ != NULL);
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// �������ݽ�����־...
	m_bFinished = true;

	// �����˳�֮�����ͷ�rtsp�����Դ...
	// ֻ����������� shutdownStream() �����ط���Ҫ����...
	if (m_rtspClient_ != NULL) {
		m_rtspClient_->shutdownStream();
		m_rtspClient_ = NULL;
	}

	// �ͷ��������...
	if (m_scheduler_ != NULL) {
		delete m_scheduler_;
		m_scheduler_ = NULL;
	}

	// �ͷŻ�������...
	if (m_env_ != NULL) {
		m_env_->reclaim();
		m_env_ = NULL;
	}
}