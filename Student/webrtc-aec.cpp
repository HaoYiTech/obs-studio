
#include "student-app.h"
#include "webrtc-aec.h"
#include "BitWritter.h"

#include "window-view-camera.hpp"
#include "echo_cancellation.h"
#include "aec_core.h"

using namespace webrtc;

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
};

CWebrtcAEC::CWebrtcAEC(CViewCamera * lpViewCamera)
  : m_lpViewCamera(lpViewCamera)
  , m_lpDecContext(NULL)
  , m_lpDecCodec(NULL)
  , m_lpDecFrame(NULL)
  , m_lpEncContext(NULL)
  , m_lpEncCodec(NULL)
  , m_lpEncFrame(NULL)
  , m_lpEchoBufNN(NULL)
  , m_lpHornBufNN(NULL)
  , m_lpMicBufNN(NULL)
  , m_lpAECSem(NULL)
  , m_nWebrtcMS(0)
  , m_nWebrtcNN(0)
  , m_hAEC(NULL)
{
	m_lpNsxInst = NULL;
	// ��ʼ����˷��������������ת����...
	m_mic_resampler = NULL;
	m_horn_resampler = NULL;
	m_echo_resampler = NULL;
	// ����Ĭ��������������������...
	m_out_channel_num = DEF_AUDIO_OUT_CHANNEL_NUM;
	m_out_sample_rate = DEF_AUDIO_OUT_SAMPLE_RATE;
	// 1024�ǲ��Ŷ�ÿ�δ���������� => 1024/16 = 64ms
	// ����AACÿ֡������ => AAC-1024 | MP3-1152
	m_aac_nb_samples = 1024;
	m_aac_frame_count = 0;
	m_aac_out_bitrate = 0;
	m_aac_frame_size = 0;
	m_aac_frame_ptr = NULL;
	// ����Ĭ�ϲ���ֵ...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	// ��ʼ���������ζ��ж���...
	circlebuf_init(&m_circle_mic);
	circlebuf_init(&m_circle_horn);
	circlebuf_init(&m_circle_echo);
	circlebuf_reserve(&m_circle_mic, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_echo, DEF_CIRCLE_SIZE / 4);
	// ��ʼ�����������������...
	pthread_mutex_init_value(&m_AECMutex);
}

CWebrtcAEC::~CWebrtcAEC()
{
	// �ȶ��߳̽����˳���־�趨�������ٴν����ź��������ȴ�...
	this->SendStopRequest();
	// �����ź����仯�¼�����ʹ�߳������˳�...
	((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ͷŽ���ṹ��...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// �ͷŽ���������...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
		m_lpDecContext = NULL;
	}
	// �ͷ�ѹ���ṹ��...
	if (m_lpEncFrame != NULL) {
		av_frame_free(&m_lpEncFrame);
		m_lpEncFrame = NULL;
	}
	// �ͷ�ѹ��������...
	if (m_lpEncContext != NULL) {
		avcodec_close(m_lpEncContext);
		av_free(m_lpEncContext);
		m_lpEncContext = NULL;
	}
	// �ͷŻ�����������...
	if (m_hAEC != NULL) {
		WebRtcAec_Free(m_hAEC);
		m_hAEC = NULL;
	}
	// �ͷŻ��������ź���...
	if (m_lpAECSem != NULL) {
		os_sem_destroy(m_lpAECSem);
		m_lpAECSem = NULL;
	}
	// �ͷ���Ƶ���ζ���...
	circlebuf_free(&m_circle_mic);
	circlebuf_free(&m_circle_horn);
	circlebuf_free(&m_circle_echo);
	// �ͷŻ�������ʹ�õ��Ļ���ռ�...
	if (m_lpMicBufNN != NULL) {
		bfree(m_lpMicBufNN);
		m_lpMicBufNN = NULL;
	}
	if (m_lpHornBufNN != NULL) {
		bfree(m_lpHornBufNN);
		m_lpHornBufNN = NULL;
	}
	if (m_lpEchoBufNN != NULL) {
		bfree(m_lpEchoBufNN);
		m_lpEchoBufNN = NULL;
	}
	if (m_pDMicBufNN != NULL) {
		bfree(m_pDMicBufNN);
		m_pDMicBufNN = NULL;
	}
	if (m_pDHornBufNN != NULL) {
		bfree(m_pDHornBufNN);
		m_pDHornBufNN = NULL;
	}
	if (m_pDEchoBufNN != NULL) {
		bfree(m_pDEchoBufNN);
		m_pDEchoBufNN = NULL;
	}
	// �ͷŽ���ģ��ʹ�õĶ���Ϳռ�...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Free(m_lpNsxInst);
		m_lpNsxInst = NULL;
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
	pthread_mutex_destroy(&m_AECMutex);
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

// �����Ҳಥ���߳��Ѿ�ֹ֪ͣͨ...
BOOL CWebrtcAEC::onUdpRecvThreadStop()
{
	// ��ʱһ��û������������...
	if (App()->GetAudioHorn())
		return false;
	ASSERT(!App()->GetAudioHorn());
	// �ͷŻ�����������...
	if (m_hAEC != NULL) {
		WebRtcAec_Free(m_hAEC);
		m_hAEC = NULL;
	}
	// ��ʼ���������������� => Webrtc
	m_hAEC = WebRtcAec_Create();
	if (m_hAEC == NULL)
		return false;
	// ���ò�ȷ����ʱ���Զ����벨�� => �����ڳ�ʼ��֮ǰ����...
	Aec * lpAEC = (Aec*)m_hAEC;
	WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
	WebRtcAec_enable_extended_filter(lpAEC->aec, true);
	// ��ʼ��������������ʧ�ܣ�ֱ�ӷ���...
	if (WebRtcAec_Init(m_hAEC, m_out_sample_rate, m_out_sample_rate) != 0)
		return false;
	// �ͷ�����������������Ƶת����...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
	// ע�⣺����������������Ӱ���µĻ�������...
	// ���³�ʼ�����������λ�����ж���...
	circlebuf_free(&m_circle_horn);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	// �ؽ�������������ɹ�...
	return true;
}

BOOL CWebrtcAEC::InitWebrtc(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum, int nOutBitrateAAC)
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

	// ��ʼ�����������ź���...
	if (os_sem_init(&m_lpAECSem, 0) != 0)
		return false;
	// �ź�����ʼ��ʧ�ܣ�ֱ�ӷ���...
	if (m_lpAECSem == NULL)
		return false;
	// ��ʼ���������������� => Webrtc
	m_hAEC = WebRtcAec_Create();
	if (m_hAEC == NULL)
		return false;
	// ���ò�ȷ����ʱ���Զ����벨�� => �����ڳ�ʼ��֮ǰ����...
	Aec * lpAEC = (Aec*)m_hAEC;
	WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
	WebRtcAec_enable_extended_filter(lpAEC->aec, true);
	// ��ʼ��������������ʧ�ܣ�ֱ�ӷ���...
	if (WebRtcAec_Init(m_hAEC, m_out_sample_rate, m_out_sample_rate) != 0)
		return false;
	// ÿ�λ���������������������Ҫ����������...
	m_nWebrtcNN = DEF_WEBRTC_AEC_NN * m_out_channel_num;
	// ����ÿ�δ����趨������ռ�õĺ�����...
	m_nWebrtcMS = m_nWebrtcNN / ((m_out_sample_rate / 1000) * m_out_channel_num);
	// ����ÿ�δ����������ݷ������������Ҫ�õ������ݿռ� => ������short��ʽ...
	// ע�⣺����ʹ��obs���ڴ����ӿڣ����Խ���й©���� => bzalloc => bfree
	m_lpMicBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_lpHornBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_lpEchoBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_pDMicBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));
	m_pDHornBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));
	m_pDEchoBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));

	// ���������˷�Ľ���ģ����� => short��ʽ...
	int err_code = 0;
	m_lpNsxInst = WebRtcNsx_Create();
	// ��ʼ���������ʹ��16kƵ��...
	if (m_lpNsxInst != NULL) {
		err_code = WebRtcNsx_Init(m_lpNsxInst, m_out_sample_rate);
	}
	// �趨����̶� => 0: Mild, 1: Medium , 2: Aggressive
	if (m_lpNsxInst != NULL && err_code == 0) {
		err_code = WebRtcNsx_set_policy(m_lpNsxInst, 2);
	}

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
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// �򿪻�ȡ���Ľ�����...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) != 0)
		return false;
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);

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
	// ������Ҫ��ѹ���������������������...
	m_lpEncCodec = avcodec_find_encoder(src_codec_id);
	m_lpEncContext = avcodec_alloc_context3(m_lpEncCodec);
	// ����ѹ��������ز�����Ϣ...
	m_lpEncContext->codec_id = src_codec_id;
	m_lpEncContext->codec_type = AVMEDIA_TYPE_AUDIO;
	m_lpEncContext->sample_fmt = enc_out_sample_fmt;
	m_lpEncContext->sample_rate = enc_out_sample_rate;
	m_lpEncContext->channels = enc_out_channel_num;
	m_lpEncContext->channel_layout = av_get_default_channel_layout(enc_out_channel_num);
	m_lpEncContext->bit_rate = m_aac_out_bitrate;
	// �򿪻�ȡ����ѹ��������...
	int errCode = avcodec_open2(m_lpEncContext, m_lpEncCodec, NULL);
	if (errCode < 0) {
		char errBuf[MAX_PATH] = { 0 };
		av_strerror(errCode, errBuf, MAX_PATH);
		blog(LOG_INFO, "[AAC-Encoder] Error => %s", errBuf);
		return false;
	}
	// AACѹ����ÿ����Ҫ����Ĳ������ݸ��� => ѹ����Ĭ�Ϸ���Ĳ������� => 1024
	m_aac_nb_samples = m_lpEncContext->frame_size;
	int enc_in_nb_samples = m_aac_nb_samples;
	int enc_out_nb_samples = av_rescale_rnd(enc_in_nb_samples, enc_out_sample_rate, enc_in_sample_rate, AV_ROUND_UP);
	// ����AACѹ������Ҫ��PCM����֡���ֽ�������Ԥ�ȷ��仺��...
	m_aac_frame_size = av_samples_get_buffer_size(NULL, enc_out_channel_num, enc_out_nb_samples, enc_out_sample_fmt, 1);
	m_aac_frame_ptr = (uint8_t *)av_malloc(m_aac_frame_size + 1);
	// ׼��һ��ȫ�ֵ�ѹ���ṹ�� => ѹ������֡���໥������...
	m_lpEncFrame = av_frame_alloc();
	ASSERT(m_lpEncFrame != NULL);

	// ������˷��������Ƶ��ʽ...
	resample_info micInfo = { 0 };
	micInfo.samples_per_sec = m_in_sample_rate;
	micInfo.speakers = convert_speaker_layout(nInChannelNum);
	micInfo.format = convert_sample_format(m_lpDecContext->sample_fmt);
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
	// �������ת������ʧ�ܣ�ֱ�ӷ���...
	if (m_mic_resampler == NULL || m_echo_resampler == NULL) {
		blog(LOG_INFO, "Error => audio_resampler_create");
		return false;
	}

	// �������������߳�...
	this->Start();

	return true;
}

void CWebrtcAEC::doSaveAudioPCM(void * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID)
{
	// ������ܽ���AEC�����Ĵ��̹�����ֱ�ӷ���...
	if (!App()->IsSaveAECSample())
		return;
	// ע�⣺PCM���ݱ����ö����Ʒ�ʽ���ļ�...
	char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "D:/MP4/PCM/IPC_%d_%d_%d.pcm", nAudioRate, nAudioChannel, nDBCameraID);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// ���ļ��ɹ�����ʼд����ƵPCM��������...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}

// ���ڻ�����WSAPI����Ƶ��ʽ�����Ǳ�׼��float��ʽ...
BOOL CWebrtcAEC::PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	// ����߳��Ѿ��˳���ֱ�ӷ���...
	if (this->IsStopRequested())
		return false;
	////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺���ﲻ������������Ͷ����Ƶ���ݣ�ֻҪ������Ч�Ϳ���Ͷ������...
	// ע�⣺ֻ������ͨ�����ܻ���������Ҳ���ǲ����ܷ���ֻ�����������ݶ�û����˷����ݵ����...
	// ע�⣺����Ҫ������Ͷ�ݣ��������˷�����������ʼ���ݲ����ϸ��������Ļ���ʱ����������...
	////////////////////////////////////////////////////////////////////////////////////////////
	// �����������ת������Ϊ�գ���Ҫ����ת����������...
	if (m_horn_resampler == nullptr) {
		resample_info hornInfo = { 0 };
		// ע��WSAPI��������Ƶ�ǹ̶���float��ʽ...
		hornInfo.samples_per_sec = nSampleRate;
		hornInfo.speakers = convert_speaker_layout(nChannelNum);
		hornInfo.format = convert_sample_format(AV_SAMPLE_FMT_FLT);
		// ����������ԭʼ���ݵ��ز������󣬽��������ɼ�������������ת���ɻ���������Ҫ��������ʽ...
		m_horn_resampler = audio_resampler_create(&m_echo_sample_info, &hornInfo);
		// �������ת������ʧ�ܣ�ֱ�ӷ���...
		if (m_horn_resampler == NULL) {
			blog(LOG_INFO, "Error => audio_resampler_create");
			return false;
		}
		// ��ӡ��һ��������Ͷ�ݵ���Ƶ����ʱ��� => ��ӡ��ǰʱ�̵���˷绺�泤��...
		blog(LOG_INFO, "== horn-aec first packet: %I64d, mic-size: %d, horn-size: %d ==", 
			 os_gettime_ns() / 1000000, m_circle_mic.size, m_circle_horn.size);
		// ��������ʽͶ����������������ʱ���Ž��б�־�趨�����ܱ��������ӳٶ�AEC��Ӱ��...
		App()->SetAudioHorn(true);
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	uint32_t  input_frames = nBufSize / (nChannelNum * sizeof(float));
	// �������ԭʼ��Ƶ������ʽ����ͳһ�ĸ�ʽת����ת���ɹ������뻷�ζ��� => ת����short��ʽ...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&lpBufData, input_frames)) {
		// �����ʽת��֮����������ݳ��ȣ�����ת��������ݷ�����黺�浱��...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		// �������⣬ѭ��ѹ�뻷�ζ��е���...
		pthread_mutex_lock(&m_AECMutex);
		circlebuf_push_back(&m_circle_horn, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_AECMutex);
		// �����ź����仯֪ͨ�����Խ��л�������������...
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
	return true;
}

// ע�⣺�����������Ƿ���Ч����Ҫ����˷����ݽ��д���...
BOOL CWebrtcAEC::PushMicFrame(FMS_FRAME & inFrame)
{
	// ����߳��Ѿ��˳���ֱ�ӷ���...
	if (this->IsStopRequested())
		return false;
	// ����������Ϊ�գ�ֱ�ӷ���ʧ��...
	if (m_lpDecContext == NULL || m_lpDecCodec == NULL || m_lpDecFrame == NULL)
		return false;
	// �����˷���Ƶת������Ч��ֱ�ӷ���...
	if (m_mic_resampler == NULL)
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
	// ע�⣺��������ĸ�ʽ��AV_SAMPLE_FMT_FLTP����Ҫ���и�ʽת��...
	nResult = avcodec_decode_audio4(m_lpDecContext, m_lpDecFrame, &got_picture, &theNewPacket);
	// ���û�н���������ݰ�����ӡ��Ϣ���ͷ����ݰ�...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
		av_free_packet(&theNewPacket);
		return false;
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// ��ԭʼ��˷���Ƶ������ʽ��ת���ɻ���������Ҫ��������ʽ��ת���ɹ����뻷�ζ��� => AV_SAMPLE_FMT_S16 => short��ʽ...
	if (audio_resampler_resample(m_mic_resampler, output_data, &output_frames, &ts_offset, m_lpDecFrame->data, m_lpDecFrame->nb_samples)) {
		// �����ʽת��֮����������ݳ��ȣ�����ת��������ݷ���ֿ黺�浱�� => ÿ����10���� => ÿ���鶼�ж�����PTS...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		// �������⣬ѭ��ѹ�뻷�ζ��е��� => �ֿ鴦��ÿ����10���룬ÿ���鶼�ж�����PTS...
		this->doPushToMic(theNewPacket.pts, output_data[0], cur_data_size);
		// �����ź����仯֪ͨ�����Խ��л�������������...
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
	// �ͷ�AVPacket���ݰ�...
	av_free_packet(&theNewPacket);
	return true;
}

void CWebrtcAEC::doPushToMic(int64_t inPTS, uint8_t * lpData, int nSize)
{
	pthread_mutex_lock(&m_AECMutex);
	// ÿ�����ݿ��������С => 10���� => AV_SAMPLE_FMT_S16 => short����...
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	// �����ϴηֿ����ռ�õĺ����� => ��Ҫ����ÿ����ռ�õ��ֽ���...
	int remain_ms = m_strRemainMic.size() * m_nWebrtcMS / nNeedBufBytes;
	// ��ʣ�������ۼӣ������㿪ʼPTS����ֵ => ��ǰ��ȥʣ�������...
	int64_t start_pts = inPTS - remain_ms;
	m_strRemainMic.append((char*)lpData, nSize);
	// �����������ɵ����ݿ���� => 10�������ݿ�ĸ���...
	int nBlockSize = m_strRemainMic.size() / nNeedBufBytes;
	const char * lpStartMic = m_strRemainMic.c_str();
	for (int i = 0; i < nBlockSize; ++i) {
		int64_t cur_pts = start_pts + i * m_nWebrtcMS;
		const char * cur_mic = lpStartMic + i * nNeedBufBytes;
		circlebuf_push_back(&m_circle_mic, &cur_pts, sizeof(int64_t));
		circlebuf_push_back(&m_circle_mic, cur_mic, nNeedBufBytes);
		//blog(LOG_INFO, "Mic-Block => PTS: %I64d, Size: %d", cur_pts, nNeedBufBytes);
	}
	// ɾ���Ѿ����뻷�ζ��е���Ƶ�������ݣ�ʣ����ǲ���10��������...
	m_strRemainMic.erase(0, nBlockSize * nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
}

void CWebrtcAEC::Entry()
{
	// �����ȴ��ź��������仯...
	while (os_sem_wait(m_lpAECSem) == 0) {
		// ����߳��Ѿ��˳����ж�ѭ��...
		if (this->IsStopRequested())
			break;
		// ���е�����˷紦��...
		this->doEchoMic();
		// ���л�����������...
		this->doEchoCancel();
		// ����AACѹ�����...
		this->doEchoEncode();
	}
}

// ��������Чʱ�Ż�ִ��...
void CWebrtcAEC::doEchoMic()
{
	// ����������ѿ�����������ֱ�ӷ���...
	if (App()->GetAudioHorn())
		return;
	// ��˷����ݱ������һ������Ԫ => ������ * ÿ������ռ���ֽ��� => AV_SAMPLE_FMT_S16...
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	if (m_circle_mic.size < (nNeedBufBytes + sizeof(int64_t)))
		return;
	int64_t frame_pts_ms = 0;
	// �ӻ��ζ��е��ж�ȡһ������ => ����10����...
	pthread_mutex_lock(&m_AECMutex);
	circlebuf_pop_front(&m_circle_mic, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
	// ����˷����ݽ��н��봦�� => short��ʽ...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Process(m_lpNsxInst, &m_lpMicBufNN, m_out_channel_num, &m_lpEchoBufNN);
		memcpy(m_lpMicBufNN, m_lpEchoBufNN, nNeedBufBytes);
	}
	// �����ݽ���ֱ��ת��...
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nWebrtcNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// ֱ�ӶԽ�������˷���Ƶ������ʽ��ת����AACѹ������Ҫ����Ƶ������ʽ��ת���ɹ����뻷�ζ��У�AV_SAMPLE_FMT_S16 => AV_SAMPLE_FMT_FLTP...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpMicBufNN, input_frames)) {
		// �Խ��������ݷ���AACѹ����������ζ��е��У�������һ����ѹ�����Ͷ�ݴ��� => ע�⣬���ﲻ��Ҫ�û������...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
		//blog(LOG_INFO, "Echo-Block => PTS: %I64d, Size: %d", frame_pts_ms, cur_data_size);
	}
	// �����˷绷�ζ������滹���㹻�����ݿ���Ҫ���д��������ź����仯�¼�...
	if (m_circle_mic.size > nNeedBufBytes) {
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
}

// ��������Чʱ���Ż�ִ��...
void CWebrtcAEC::doEchoCancel()
{
	// ���������û�п�����������ֱ�ӷ���...
	if (!App()->GetAudioHorn())
		return;
	// ��˷�����������������һ������Ԫ => ������ * ÿ������ռ���ֽ��� => AV_SAMPLE_FMT_S16
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	if (m_circle_mic.size < (nNeedBufBytes + sizeof(int64_t)) || m_circle_horn.size < nNeedBufBytes)
		return;
	int err_code = 0;
	int64_t frame_pts_ms = 0;
	// ����˷�����������ζ�����ͬʱ��ȡ����...
	pthread_mutex_lock(&m_AECMutex);
	circlebuf_pop_front(&m_circle_mic, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	circlebuf_pop_front(&m_circle_horn, m_lpHornBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
	// ��short���ݸ�ʽת����float���ݸ�ʽ...
	for (int i = 0; i < m_nWebrtcNN; ++i) {
		m_pDMicBufNN[i] = (float)m_lpMicBufNN[i];
		m_pDHornBufNN[i] = (float)m_lpHornBufNN[i];
	}
	// ע�⣺ʹ���Զ�������ʱģʽ�����Խ�msInSndCardBuf��������Ϊ0...
	// �Ƚ����������ݽ���Զ��Ͷ�ݣ���Ͷ����˷����ݽ��л������� => Ͷ����������...
	err_code = WebRtcAec_BufferFarend(m_hAEC, m_pDHornBufNN, m_nWebrtcNN);
	err_code = WebRtcAec_Process(m_hAEC, &m_pDMicBufNN, m_out_channel_num, &m_pDEchoBufNN, m_nWebrtcNN, 0, 0);
	// ��float���ݸ�ʽת����short���ݸ�ʽ...
	for (int i = 0; i < m_nWebrtcNN; ++i) {
		m_lpEchoBufNN[i] = (short)m_pDEchoBufNN[i];
	}
	// ����˷��PCM���ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	this->doSaveAudioPCM((void*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0, m_lpViewCamera->GetDBCameraID());
	// ����������PCM���ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	this->doSaveAudioPCM((void*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1, m_lpViewCamera->GetDBCameraID());
	// �Ի�������������ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	this->doSaveAudioPCM((void*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2, m_lpViewCamera->GetDBCameraID());
	// �Ի�������������ݽ��н��봦�� => short��ʽ...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Process(m_lpNsxInst, &m_lpEchoBufNN, m_out_channel_num, &m_lpMicBufNN);
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// �Խ����Ļ����������ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	this->doSaveAudioPCM((void*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 3, m_lpViewCamera->GetDBCameraID());
	// �������󣬴�ӡ������Ϣ...
	if (err_code != 0) {
		blog(LOG_INFO, "== err: %d, mic_buf: %d, horn_buf: %d ==", err_code, m_circle_mic.size, m_circle_horn.size);
	}

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nWebrtcNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// �Ի������������Ƶ������ʽ��ת����AACѹ������Ҫ����Ƶ������ʽ��ת���ɹ����뻷�ζ���...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpEchoBufNN, input_frames)) {
		// �Ի�������������ݷ���AACѹ����������ζ��е��У�������һ����ѹ�����Ͷ�ݴ��� => ע�⣬���ﲻ��Ҫ�û������...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
	}

	// �����˷�����������ζ������滹���㹻�����ݿ���Ҫ���л��������������ź����仯�¼�...
	if (m_circle_mic.size > nNeedBufBytes && m_circle_horn.size >= nNeedBufBytes) {
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
}

void CWebrtcAEC::doEchoEncode()
{
	// ���� m_nWebrtcNN ��Ӧ��ʵ���������� => m_nWebrtcNN������������ => ���ڲ�����û�б仯�������������Ҳ�����б仯...
	int nb_samples = av_rescale_rnd(m_nWebrtcNN, m_aac_sample_info.samples_per_sec, m_echo_sample_info.samples_per_sec, AV_ROUND_UP);
	// �����������ݱ������һ������Ԫ => ������ * ÿ������ռ���ֽ��� => AV_SAMPLE_FMT_FLTP
	int nPerSampleByte = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP);
	int nNeedBufBytes = nb_samples * nPerSampleByte;
	if (m_circle_echo.size < (nNeedBufBytes + sizeof(int64_t)))
		return;
	int64_t frame_pts_ms = 0;
	int cur_frame_size = nNeedBufBytes;
	// �ӻ���������Ķ�����ȡ��һ��10������Ƶ����...
	circlebuf_pop_front(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_echo, m_aac_frame_ptr, cur_frame_size);
	ASSERT(cur_frame_size < m_aac_frame_size);
	// ���㵱ǰʣ�໺�����ݿ��Գ������ŵĺ����� => ʣ�����ֽ��� / (ÿ���������� * ÿ�����ֽ���)
	int64_t aac_cur_pts = frame_pts_ms - m_strRemainEcho.size() / ((m_out_sample_rate / 1000) * m_out_channel_num * nPerSampleByte);
	// ����ȡ���ĵ���Ƶ����׷�ӵ�ʣ�໺�����ݵ�ĩβ...
	m_strRemainEcho.append((char*)m_aac_frame_ptr, cur_frame_size);
	// �����Ҫ��ԭʼ��Ƶ���治�������صȴ�...
	if (m_strRemainEcho.size() < m_aac_frame_size)
		return;
	// ��ȡ��ѹ����Ƶ���ݵ�ͷָ��λ��...
	ASSERT(m_strRemainEcho.size() >= m_aac_frame_size);
	uint8_t * lpEchoData = (uint8_t*)m_strRemainEcho.c_str();
	// �ۼ�AAC��ѹ��֡������...
	++m_aac_frame_count;
	// ΪAVFrame׼����ص����͸�ʽ...
	m_lpEncFrame->nb_samples = m_lpEncContext->frame_size;
	m_lpEncFrame->format = m_lpEncContext->sample_fmt;
	m_lpEncFrame->pts = aac_cur_pts;
	// ʹ��ԭʼ��PCM�������AVFrame��׼����ʼѹ��...
	int nFillSize = avcodec_fill_audio_frame(m_lpEncFrame, m_lpEncContext->channels, m_lpEncContext->sample_fmt, lpEchoData, m_aac_frame_size, 1);
	// ɾ���Ѿ�ѹ������Ƶ�������ݣ�ʣ����ǲ���10��������...
	if (nFillSize < 0) {
		m_strRemainEcho.erase(0, m_aac_frame_size);
		return;
	}
	int got_frame = 0;
	AVPacket pkt = { 0 };
	// �����µ�ѹ����������PCM����ѹ����AAC����...
	int nResult = avcodec_encode_audio2(m_lpEncContext, &pkt, m_lpEncFrame, &got_frame);
	// ѹ��ʧ�ܻ�û�еõ�����AAC����֡��ɾ���Ѿ�ѹ������Ƶ��������...
	if (nResult < 0 || !got_frame) {
		m_strRemainEcho.erase(0, m_aac_frame_size);
		return;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// ���㵱ǰAACѹ���������֡��PTS => ����ʱ����ļ��㷽ʽ���ھ޴�����...
	// m_aac_frame_count ����һ����ȷ��ʱ�䳤�˻�������ص��ӳ�����...
	// �µ�ģ�ͣ�ʼ�ղ���IPCͶ�ݹ�����ԭʼʱ���������Ƶʱ�������һ��...
	// int64_t aac_cur_pts = m_mic_aac_ms_zero + m_aac_frame_count * aac_frame_duration;
	///////////////////////////////////////////////////////////////////////////////////

	// ��ӡAACѹ����Ľ����Ϣ��Ͷ��PTS������ʹ�� pkt.pts����Ϊ���տ�ʼ��ʱ������ܲ���ȷ...
	//blog(LOG_INFO, "Encode-AAC => inPTS: %I64d, inSize: %d, outPTS: %I64d, outSize: %d", aac_cur_pts, m_aac_frame_size, pkt.pts, pkt.size);

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
	// ɾ���Ѿ�ѹ������Ƶ�������ݣ�ʣ����ǲ���10��������...
	m_strRemainEcho.erase(0, m_aac_frame_size);
}