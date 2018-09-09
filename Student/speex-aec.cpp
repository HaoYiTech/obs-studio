
#include "speex-aec.h"
#include "BitWritter.h"

#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#include "window-view-camera.hpp"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
};

CSpeexAEC::CSpeexAEC(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
  , m_lpDeContext(nullptr)
  , m_lpDeCodec(nullptr)
  , m_lpDeFrame(nullptr)
  , m_lpEnContext(nullptr)
  , m_lpEnCodec(nullptr)
  , m_lpEnFrame(nullptr)
  , m_lpSpeexEcho(NULL)
  , m_lpSpeexDen(NULL)
  , m_lpSpeexSem(NULL)
  , m_lpEchoBufNN(NULL)
  , m_lpHornBufNN(NULL)
  , m_lpMicBufNN(NULL)
  , m_horn_delay_ms(0)
  , m_nSpeexTAIL(0)
  , m_nSpeexNN(0)
{
	// ��ʼ����˷��������������ת����...
	m_mic_resampler = NULL;
	m_horn_resampler = NULL;
	m_echo_resampler = NULL;
	// ����AACÿ֡������...
	m_aac_nb_samples = 1024;
	m_aac_frame_count = 0;
	m_aac_out_bitrate = 0;
	m_aac_frame_size = 0;
	m_aac_frame_ptr = NULL;
	// ����Ĭ�ϲ���ֵ...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	// ����Ĭ��������������������...
	m_out_channel_num = 1;
	m_out_sample_rate = 8000;
	// ����Ĭ�ϵ�0��ʱ�̵�...
	m_mic_pts_ms_zero = -1;
	m_mic_sys_ns_zero = -1;
	m_horn_sys_ns_zero = -1;
	// ��ʼ���������ζ��ж���...
	circlebuf_init(&m_circle_mic);
	circlebuf_init(&m_circle_horn);
	circlebuf_init(&m_circle_echo);
	circlebuf_reserve(&m_circle_mic, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_echo, DEF_CIRCLE_SIZE / 4);
	// ��ʼ�����������������...
	pthread_mutex_init_value(&m_SpeexMutex);
}

CSpeexAEC::~CSpeexAEC()
{
	// �ȶ��߳̽����˳���־�趨�������ٴν����ź��������ȴ�...
	this->SendStopRequest();
	// �����ź����仯�¼�����ʹ�߳������˳�...
	((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ͷŽ���ṹ��...
	if (m_lpDeFrame != NULL) {
		av_frame_free(&m_lpDeFrame);
		m_lpDeFrame = NULL;
	}
	// �ͷŽ���������...
	if (m_lpDeContext != nullptr) {
		avcodec_close(m_lpDeContext);
		av_free(m_lpDeContext);
		m_lpDeContext = NULL;
	}
	// �ͷ�ѹ���ṹ��...
	if (m_lpEnFrame != NULL) {
		av_frame_free(&m_lpEnFrame);
		m_lpEnFrame = NULL;
	}
	// �ͷ�ѹ��������...
	if (m_lpEnContext != nullptr) {
		avcodec_close(m_lpEnContext);
		av_free(m_lpEnContext);
		m_lpEnContext = NULL;
	}
	// �ͷŻ���������ض���...
	if (m_lpSpeexEcho != NULL) {
		speex_echo_state_destroy(m_lpSpeexEcho);
		m_lpSpeexEcho = NULL;
	}
	if (m_lpSpeexDen != NULL) {
		speex_preprocess_state_destroy(m_lpSpeexDen);
		m_lpSpeexDen = NULL;
	}
	// �ͷŻ��������ź���...
	if (m_lpSpeexSem != NULL) {
		os_sem_destroy(m_lpSpeexSem);
		m_lpSpeexSem = NULL;
	}
	// �ͷ���Ƶ���ζ���...
	circlebuf_free(&m_circle_mic);
	circlebuf_free(&m_circle_horn);
	circlebuf_free(&m_circle_echo);
	// �ͷŻ�������ʹ�õ��Ļ���ռ�...
	if (m_lpMicBufNN != NULL) {
		delete[] m_lpMicBufNN;
		m_lpMicBufNN = NULL;
	}
	if (m_lpHornBufNN != NULL) {
		delete[] m_lpHornBufNN;
		m_lpHornBufNN = NULL;
	}
	if (m_lpEchoBufNN != NULL) {
		delete[] m_lpEchoBufNN;
		m_lpEchoBufNN = NULL;
	}
	// �ͷ���˷����������Ƶת����...
	if (m_mic_resampler != nullptr) {
		audio_resampler_destroy(m_mic_resampler);
		m_mic_resampler = nullptr;
	}
	// �ͷ�����������������Ƶת����...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
	// �ͷŻ���������Ƶת����...
	if (m_echo_resampler != nullptr) {
		audio_resampler_destroy(m_echo_resampler);
		m_echo_resampler = nullptr;
	}
	// �ͷ�AACѹ��ԭʼPCM����ռ�...
	if (m_aac_frame_ptr != nullptr) {
		av_free(m_aac_frame_ptr);
		m_aac_frame_ptr = NULL;
	}
	// �ͷŻ��������������...
	pthread_mutex_destroy(&m_SpeexMutex);
}

static inline enum audio_format convert_sample_format(int f)
{
	switch (f) {
	case AV_SAMPLE_FMT_U8:   return AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_S16:  return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:  return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_FLT:  return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:  return AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P: return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P: return AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP: return AUDIO_FORMAT_FLOAT_PLANAR;
	default:;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:     return SPEAKERS_UNKNOWN;
	case 1:     return SPEAKERS_MONO;
	case 2:     return SPEAKERS_STEREO;
	case 3:     return SPEAKERS_2POINT1;
	case 4:     return SPEAKERS_4POINT0;
	case 5:     return SPEAKERS_4POINT1;
	case 6:     return SPEAKERS_5POINT1;
	case 8:     return SPEAKERS_7POINT1;
	default:    return SPEAKERS_UNKNOWN;
	}
}

BOOL CSpeexAEC::InitSpeex(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum,
                          int nOutBitrateAAC, int nHornDelayMS, int nSpeexFrameMS, int nSpeexFilterMS)
{
	// ����������ȡ������...
	switch (nInRateIndex)
	{
	case 0x03: m_in_sample_rate = 48000; break;
	case 0x04: m_in_sample_rate = 44100; break;
	case 0x05: m_in_sample_rate = 32000; break;
	case 0x06: m_in_sample_rate = 24000; break;
	case 0x07: m_in_sample_rate = 22050; break;
	case 0x08: m_in_sample_rate = 16000; break;
	case 0x09: m_in_sample_rate = 12000; break;
	case 0x0a: m_in_sample_rate = 11025; break;
	case 0x0b: m_in_sample_rate = 8000; break;
	default:   m_in_sample_rate = 48000; break;
	}
	// ����AACѹ���������...
	m_aac_out_bitrate = nOutBitrateAAC;
	// �������������������...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	// ������������ʺ��������...
	m_out_sample_rate = nOutSampleRate;
	m_out_channel_num = nOutChannelNum;
	// �������趨�Ĳ����ӳ�ʱ��(����)...
	m_horn_delay_ms = nHornDelayMS;
	// ����ÿ�δ����趨������ռ�õ�������(Ĭ��16����)
	m_nSpeexNN = nSpeexFrameMS * (m_out_sample_rate / 1000) * m_out_channel_num;
	// ����β��Ϊ�趨����ռ�õ�������(Ĭ��400����) => �����ͬ����Խ�󣬼�����Խ������Ч��Խ��...
	m_nSpeexTAIL = nSpeexFilterMS * (m_out_sample_rate / 1000) * m_out_channel_num;
	// ���ݼ������������������Ҫ�õ������ݿռ�...
	m_lpMicBufNN = new short[m_nSpeexNN];
	m_lpHornBufNN = new short[m_nSpeexNN];
	m_lpEchoBufNN = new short[m_nSpeexNN];
	// ��ʼ�����������ź���...
	if (os_sem_init(&m_lpSpeexSem, 0) != 0)
		return false;
	// �ź�����ʼ��ʧ�ܣ�ֱ�ӷ���...
	if (m_lpSpeexSem == NULL)
		return false;
	// ��ʼ���������������� => speex
 	m_lpSpeexEcho = speex_echo_state_init(m_nSpeexNN, m_nSpeexTAIL);
	m_lpSpeexDen = speex_preprocess_state_init(m_nSpeexNN, m_out_sample_rate);
	speex_echo_ctl(m_lpSpeexEcho, SPEEX_ECHO_SET_SAMPLING_RATE, &m_out_sample_rate);
	speex_preprocess_ctl(m_lpSpeexDen, SPEEX_PREPROCESS_SET_ECHO_STATE, m_lpSpeexEcho);

	// ��ʼ��ffmpeg...
	av_register_all();

	// ���ý�������ѹ������Ҫ�Ĺ�ͬ����...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ����ѹ������Ҫת����PCM��Ƶ���ݸ�ʽ => �������� ת AACѹ��ǰ����...
	// (Decoder)AV_SAMPLE_FMT_FLTP => (EchoCancel)AV_SAMPLE_FMT_S16 => (Encoder)AV_SAMPLE_FMT_FLTP
	AVSampleFormat echo_sample_fmt = AV_SAMPLE_FMT_S16;
	AVSampleFormat aac_sample_fmt = AV_SAMPLE_FMT_FLTP;

	/////////////////////////////////////////////////////////////////
	// ��ʼ��AAC����������...
	/////////////////////////////////////////////////////////////////
	// ������Ҫ�Ľ����������������������...
	m_lpDeCodec = avcodec_find_decoder(src_codec_id);
	m_lpDeContext = avcodec_alloc_context3(m_lpDeCodec);
	// �򿪻�ȡ���Ľ�����...
	if (avcodec_open2(m_lpDeContext, m_lpDeCodec, NULL) != 0)
		return false;
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDeFrame = av_frame_alloc();
	ASSERT(m_lpDeFrame != NULL);

	/*// ���������Ƶ�������� => AAC-1024 | MP3-1152
	// ע�⣺��û�п�ʼת����swr_get_delay()����0��out_nb_samples���������������������...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + m_aac_nb_samples;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// �������ÿ֡����������(�������޹�)��ÿ֡�ֽ���(�������й�) => ֻ��Ҫ����һ�ξ͹���...
	int out_frame_duration = out_nb_samples * 1000 / out_sample_rate;
	int out_frame_bytes = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);*/

	/////////////////////////////////////////////////////////////////
	// ��ʼ��AACѹ��������...
	/////////////////////////////////////////////////////////////////
	// ����ѹ������Ҫת����PCM��Ƶ���ݸ�ʽ => �������� ת AACѹ��ǰ����...
	AVSampleFormat enc_in_sample_fmt = echo_sample_fmt;
	AVSampleFormat enc_out_sample_fmt = aac_sample_fmt;
	// ����ѹ������Ҫת���������� => ����������...
	int enc_in_channel_num = m_out_channel_num;
	int enc_out_channel_num = m_out_channel_num;
	// ����ѹ������Ҫת����������������� => �����ʲ���...
	int enc_in_sample_rate = m_out_sample_rate;
	int enc_out_sample_rate = m_out_sample_rate;
	// ���������������Ƶ��������...
	int enc_in_nb_samples = m_aac_nb_samples;
	int enc_out_nb_samples = av_rescale_rnd(enc_in_nb_samples, enc_out_sample_rate, enc_in_sample_rate, AV_ROUND_UP);
	// ����AACѹ������Ҫ��PCM����֡���ֽ�������Ԥ�ȷ��仺��...
	m_aac_frame_size = av_samples_get_buffer_size(NULL, enc_out_channel_num, enc_out_nb_samples, enc_out_sample_fmt, 1);
	m_aac_frame_ptr = (uint8_t *)av_malloc(m_aac_frame_size + 1);
	// ������Ҫ��ѹ���������������������...
	m_lpEnCodec = avcodec_find_encoder(src_codec_id);
	m_lpEnContext = avcodec_alloc_context3(m_lpEnCodec);
	// ����ѹ��������ز�����Ϣ...
	m_lpEnContext->codec_id = src_codec_id;
	m_lpEnContext->codec_type = AVMEDIA_TYPE_AUDIO;
	m_lpEnContext->sample_fmt = enc_out_sample_fmt;
	m_lpEnContext->sample_rate = enc_out_sample_rate;
	m_lpEnContext->channels = enc_out_channel_num;
	m_lpEnContext->channel_layout = av_get_default_channel_layout(enc_out_channel_num);
	m_lpEnContext->bit_rate = m_aac_out_bitrate;
	// �򿪻�ȡ����ѹ��������...
	int errCode = avcodec_open2(m_lpEnContext, m_lpEnCodec, NULL);
	if (errCode < 0) {
		char errBuf[MAX_PATH] = { 0 };
		av_strerror(errCode, errBuf, MAX_PATH);
		blog(LOG_INFO, "[AAC-Encoder] Error => %s", errBuf);
		return false;
	}
	// ׼��һ��ȫ�ֵ�ѹ���ṹ�� => ѹ������֡���໥������...
	m_lpEnFrame = av_frame_alloc();
	ASSERT(m_lpEnFrame != NULL);

	// ������˷��������Ƶ��ʽ...
	resample_info micInfo = { 0 };
	micInfo.samples_per_sec = m_in_sample_rate;
	micInfo.speakers = convert_speaker_layout(nInChannelNum);
	micInfo.format = convert_sample_format(m_lpDeContext->sample_fmt);
	// ����AACѹ����Ҫ����Ƶ��ʽ...
	m_aac_sample_info.samples_per_sec = nOutSampleRate;
	m_aac_sample_info.speakers = convert_speaker_layout(nOutChannelNum);
	m_aac_sample_info.format = convert_sample_format(aac_sample_fmt);
	// ���û�������֮�����Ƶ��ʽ...
	m_echo_sample_info.samples_per_sec = nOutSampleRate;
	m_echo_sample_info.speakers = convert_speaker_layout(nOutChannelNum);
	m_echo_sample_info.format = convert_sample_format(echo_sample_fmt);
	// ������˷�ԭʼ���ݵ��ز������󣬽���˷�ɼ�������������ת���ɻ���������Ҫ��������ʽ...
	m_mic_resampler = audio_resampler_create(&m_echo_sample_info, &micInfo);
	// ����AACѹ��ԭʼ���ݵ��ز������󣬽����������ɼ�������������ת����AACѹ����Ҫ��������ʽ...
	m_echo_resampler = audio_resampler_create(&m_aac_sample_info, &m_echo_sample_info);

	// �������������߳�...
	this->Start();
	return true;
}

void CSpeexAEC::doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID)
{
	// ע�⣺PCM���ݱ����ö����Ʒ�ʽ���ļ�...
	char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "F:/MP4/PCM/IPC_%d_%d_%d.pcm", nAudioRate, nAudioChannel, nDBCameraID);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// ���ļ��ɹ�����ʼд����ƵPCM��������...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}

// ���ڻ�����WSAPI����Ƶ��ʽ�����Ǳ�׼��float��ʽ...
BOOL CSpeexAEC::PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum)
{
	// ����߳��Ѿ��˳���ֱ�ӷ���...
	if (this->IsStopRequested())
		return false;
	// �����׽�豸��û�г�ʼ����ϣ����ܽ��л��������������...
	if (m_mic_resampler == nullptr)
		return false;
	// �����������ת������Ϊ�գ���Ҫ����ת����������...
	if (m_horn_resampler == nullptr) {
		resample_info hornInfo = { 0 };
		// ע��WSAPI��������Ƶ�ǹ̶���float��ʽ...
		hornInfo.samples_per_sec = nSampleRate;
		hornInfo.speakers = convert_speaker_layout(nChannelNum);
		hornInfo.format = convert_sample_format(AV_SAMPLE_FMT_FLT);
		// ����������ԭʼ���ݵ��ز������󣬽��������ɼ�������������ת���ɻ���������Ҫ��������ʽ...
		m_horn_resampler = audio_resampler_create(&m_echo_sample_info, &hornInfo);
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	uint32_t  input_frames = nBufSize / (nChannelNum * sizeof(float));
	// �������ԭʼ��Ƶ������ʽ����ͳһ�ĸ�ʽת����ת���ɹ������뻷�ζ���...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&lpBufData, input_frames)) {
		// ע�⣺������Ҫ���л��Ᵽ�����Ƕ��߳����ݷ���...
		pthread_mutex_lock(&m_SpeexMutex);
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_horn, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_SpeexMutex);
		// �����ź����仯֪ͨ�����Խ��л�������������...
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}
	// ����ǵ�һ�����ݰ�����¼������ϵͳ0��ʱ��...
	if (m_horn_sys_ns_zero < 0) {
		m_horn_sys_ns_zero = os_gettime_ns();
	}
	return true;

	// ����ǵ�һ�����ݰ�����¼������ϵͳ0��ʱ��...
	/*pthread_mutex_lock(&m_SpeexMutex);
	if (m_horn_sys_ns_zero < 0) {
		// ÿ���������� * �������� * ��ʱ������ * ������...
		int nDelayBytes = (m_out_sample_rate / 1000) * sizeof(short) * m_horn_delay_ms * m_out_channel_num;
		circlebuf_push_back_zero(&m_circle_horn, nDelayBytes);
		m_horn_sys_ns_zero = os_gettime_ns();
	}
	// ����������Ƶ�������ݷ��뻷�ζ��У�ֻ�洢PCM��������...
	circlebuf_push_back(&m_circle_horn, lpBufData, nBufSize);
	pthread_mutex_unlock(&m_SpeexMutex);
	// �����ź����仯֪ͨ�����Խ��л�������������...
	((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	return true;*/
}

BOOL CSpeexAEC::PushMicFrame(FMS_FRAME & inFrame)
{
	// ����߳��Ѿ��˳���ֱ�ӷ���...
	if (this->IsStopRequested())
		return false;
	// ����������Ϊ�գ�ֱ�ӷ���ʧ��...
	if (m_lpDeContext == NULL || m_lpDeCodec == NULL || m_lpDeFrame == NULL)
		return false;
	// ����ADTS���ݰ�ͷ...
	string & inData = inFrame.strData;
 	uint32_t inPTS = inFrame.dwSendTime;
	uint32_t inOffset = inFrame.dwRenderOffset;
	bool bIsKeyFrame = inFrame.is_keyframe;
	// �ȼ���ADTSͷ���ټ�������֡����...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// ����ADTS֡ͷ...
	PutBitContext pb;
	char pbuf[ADTS_HEADER_SIZE * 2] = { 0 };
	init_put_bits(&pb, pbuf, ADTS_HEADER_SIZE);
	/* adts_fixed_header */
	put_bits(&pb, 12, 0xfff);   /* syncword */
	put_bits(&pb, 1, 0);        /* ID */
	put_bits(&pb, 2, 0);        /* layer */
	put_bits(&pb, 1, 1);        /* protection_absent */
	put_bits(&pb, 2, 2);		/* profile_objecttype */
	put_bits(&pb, 4, m_in_rate_index);
	put_bits(&pb, 1, 0);        /* private_bit */
	put_bits(&pb, 3, m_in_channel_num); /* channel_configuration */
	put_bits(&pb, 1, 0);        /* original_copy */
	put_bits(&pb, 1, 0);        /* home */
								/* adts_variable_header */
	put_bits(&pb, 1, 0);        /* copyright_identification_bit */
	put_bits(&pb, 1, 0);        /* copyright_identification_start */
	put_bits(&pb, 13, nTotalSize); /* aac_frame_length */
	put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
	put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

	flush_put_bits(&pb);

	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// �����֡ͷ���ݣ������֡������Ϣ...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	int got_picture = 0, nResult = 0;
	// ע�⣺��������ĸ�ʽ��4bit����Ҫת����16bit������swr_convert
	nResult = avcodec_decode_audio4(m_lpDeContext, m_lpDeFrame, &got_picture, &theNewPacket);
	// ���û�н���������ݰ�����ӡ��Ϣ���ͷ����ݰ�...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
		av_free_packet(&theNewPacket);
		return false;
	}

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// ��ԭʼ��˷���Ƶ������ʽ��ת���ɻ���������Ҫ��������ʽ��ת���ɹ����뻷�ζ���...
	if (audio_resampler_resample(m_mic_resampler, output_data, &output_frames, &ts_offset, m_lpDeFrame->data, m_lpDeFrame->nb_samples)) {
		pthread_mutex_lock(&m_SpeexMutex);
		// �����������֮����������ݳ��ȣ�����ת��������ݷ��뻷�ζ��е���...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_mic, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_SpeexMutex);
		// ����ǵ�һ�����ݰ�����Ҫ��¼ϵͳ0��ʱ��...
		if (m_mic_sys_ns_zero < 0) {
			m_mic_sys_ns_zero = os_gettime_ns();
		}
		// ����ǵ�һ�����ݰ�����Ҫ��¼PTS��0��ʱ��...
		if (m_mic_pts_ms_zero < 0) {
			m_mic_pts_ms_zero = theNewPacket.dts;
		}
		// �����ź����仯֪ͨ�����Խ��л�������������...
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}

	// �ͷ�AVPacket���ݰ�...
	av_free_packet(&theNewPacket);
	return true;
}

void CSpeexAEC::Entry()
{
	// �����ȴ��ź��������仯...
	while (os_sem_wait(m_lpSpeexSem) == 0) {
		// ����߳��Ѿ��˳����ж�ѭ��...
		if (this->IsStopRequested())
			break;
		// ���л�����������...
		this->doEchoCancel();
		// ����AACѹ�����...
		this->doEncodeAAC();
	}
}

void CSpeexAEC::doEchoCancel()
{
	// ע�⣺���ﲢ�����������������Ƿ��㹻���������ܽ�����˷���ʱ...
	// ��˷����ݱ������һ������Ԫ => ������ * ÿ������ռ���ֽ���...
	int nNeedBufBytes = m_nSpeexNN * sizeof(short);
	if (m_circle_mic.size < nNeedBufBytes)
		return;
	// ��ȡ��˷���Ҫ�Ĵ����������� => ע���Ƕ�ȡ�ֽ���������������...
	pthread_mutex_lock(&m_SpeexMutex);
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_SpeexMutex);
	// ��������������㹻����ȡ���������ݵ��������������浱��..
	if (m_circle_horn.size >= nNeedBufBytes) {
		// �����������ζ��е��ж�ȡ���ݵ������������ռ䵱��...
		pthread_mutex_lock(&m_SpeexMutex);
		circlebuf_pop_front(&m_circle_horn, m_lpHornBufNN, nNeedBufBytes);
		pthread_mutex_unlock(&m_SpeexMutex);
		// ��˷����������������׼���������Ƚ��л����������ٽ���һ�����⴦��...
		speex_echo_cancellation(m_lpSpeexEcho, m_lpMicBufNN, m_lpHornBufNN, m_lpEchoBufNN);
		speex_preprocess_run(m_lpSpeexDen, m_lpEchoBufNN);
		// ����������PCM���ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
		//this->doSaveAudioPCM((uint8_t*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1, m_lpViewCamera->GetDBCameraID());
	} else {
		// ���������ݲ���������˷���������ֱ�ӵ��ɻ������������������...
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// �Ի�������������ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	//this->doSaveAudioPCM((uint8_t*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0, m_lpViewCamera->GetDBCameraID());
	//this->doSaveAudioPCM((uint8_t*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2, m_lpViewCamera->GetDBCameraID());

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nSpeexNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// �Ի������������Ƶ������ʽ��ת����AACѹ������Ҫ����Ƶ������ʽ��ת���ɹ����뻷�ζ���...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpEchoBufNN, input_frames)) {
		// �Ի�������������ݷ���AACѹ����������ζ��е��У�������һ����ѹ�����Ͷ�ݴ��� => ע�⣬���ﲻ��Ҫ�û������...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
	}

	// �����˷绷�ζ������滹���㹻�����ݿ���Ҫ���л��������������ź����仯�¼�...
	if (m_circle_mic.size >= nNeedBufBytes) {
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}
}

void CSpeexAEC::doEncodeAAC()
{
	// ����������Ļ��ζ��л��治����ֱ�ӷ���...
	if (m_circle_echo.size < m_aac_frame_size)
		return;
	// ����AAC��֡�����ĺ������������ۼ�ʱ���...
	int aac_frame_duration = m_aac_nb_samples * 1000 / m_out_sample_rate;
	// �ӻ��ζ����г�ȡAACѹ������Ҫ���������� => ����Ҫ������󣬶���˳��ִ��...
	circlebuf_pop_front(&m_circle_echo, m_aac_frame_ptr, m_aac_frame_size);
	// ����AACѹ������������Ƶ��ѹ������...
	// ���㵱ǰAACѹ���������֡��PTS...
	int64_t aac_cur_pts = m_mic_pts_ms_zero + m_aac_frame_count * aac_frame_duration;
	// �ۼ�AAC��ѹ��֡������...
	++m_aac_frame_count;
	// ΪAVFrame׼����ص����͸�ʽ...
	m_lpEnFrame->nb_samples = m_lpEnContext->frame_size;
	m_lpEnFrame->format = m_lpEnContext->sample_fmt;
	m_lpEnFrame->pts = aac_cur_pts;
	// ʹ��ԭʼ��PCM�������AVFrame��׼����ʼѹ��...
	int nFillSize = avcodec_fill_audio_frame(m_lpEnFrame, m_lpEnContext->channels, m_lpEnContext->sample_fmt, m_aac_frame_ptr, m_aac_frame_size, 1);
	if (nFillSize < 0)
		return;
	int got_frame = 0;
	AVPacket pkt = { 0 };
	// �����µ�ѹ����������PCM����ѹ����AAC����...
	int nResult = avcodec_encode_audio2(m_lpEnContext, &pkt, m_lpEnFrame, &got_frame);
	// ѹ��ʧ�ܻ�û�еõ�����AAC����֡��ֱ�ӷ��أ�AVPacketû������...
	if (nResult < 0 || !got_frame)
		return;
	// ���췵����Ҫ������֡...
	FMS_FRAME theAACFrame;
	theAACFrame.strData.assign((char*)pkt.data, pkt.size);
	theAACFrame.typeFlvTag = FLV_TAG_TYPE_AUDIO;
	theAACFrame.dwSendTime = aac_cur_pts;
	theAACFrame.dwRenderOffset = 0;
	theAACFrame.is_keyframe = true;
	// ��ѹ�����AAC����֡����Ͷ�ݴ���...
	m_lpViewCamera->doPushAudioAEC(theAACFrame);
	// �ͷ�ffmpeg�ڲ��������Դ...
	av_packet_unref(&pkt);
}