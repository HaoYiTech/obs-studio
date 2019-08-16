
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
	// 初始化麦克风和扬声器的样本转换器...
	m_mic_resampler = NULL;
	m_horn_resampler = NULL;
	m_echo_resampler = NULL;
	// 设置默认输出声道、输出采样率...
	m_out_channel_num = DEF_AUDIO_OUT_CHANNEL_NUM;
	m_out_sample_rate = DEF_AUDIO_OUT_SAMPLE_RATE;
	// 1024是播放端每次处理采样个数 => 1024/16 = 64ms
	// 设置AAC每帧样本数 => AAC-1024 | MP3-1152
	m_aac_nb_samples = 1024;
	m_aac_frame_count = 0;
	m_aac_out_bitrate = 0;
	m_aac_frame_size = 0;
	m_aac_frame_ptr = NULL;
	// 设置默认参数值...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	// 初始化各个环形队列对象...
	circlebuf_init(&m_circle_mic);
	circlebuf_init(&m_circle_horn);
	circlebuf_init(&m_circle_echo);
	circlebuf_reserve(&m_circle_mic, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_echo, DEF_CIRCLE_SIZE / 4);
	// 初始化回音消除互斥对象...
	pthread_mutex_init_value(&m_AECMutex);
}

CWebrtcAEC::~CWebrtcAEC()
{
	// 先对线程进行退出标志设定，避免再次进入信号量阻塞等待...
	this->SendStopRequest();
	// 发起信号量变化事件，迫使线程主动退出...
	((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 释放解码结构体...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// 释放解码器对象...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
		m_lpDecContext = NULL;
	}
	// 释放压缩结构体...
	if (m_lpEncFrame != NULL) {
		av_frame_free(&m_lpEncFrame);
		m_lpEncFrame = NULL;
	}
	// 释放压缩器对象...
	if (m_lpEncContext != NULL) {
		avcodec_close(m_lpEncContext);
		av_free(m_lpEncContext);
		m_lpEncContext = NULL;
	}
	// 释放回音消除对象...
	if (m_hAEC != NULL) {
		WebRtcAec_Free(m_hAEC);
		m_hAEC = NULL;
	}
	// 释放回声消除信号量...
	if (m_lpAECSem != NULL) {
		os_sem_destroy(m_lpAECSem);
		m_lpAECSem = NULL;
	}
	// 释放音频环形队列...
	circlebuf_free(&m_circle_mic);
	circlebuf_free(&m_circle_horn);
	circlebuf_free(&m_circle_echo);
	// 释放回音消除使用到的缓存空间...
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
	// 释放降噪模块使用的对象和空间...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Free(m_lpNsxInst);
		m_lpNsxInst = NULL;
	}
	// 释放麦克风回音消除音频转换器...
	if (m_mic_resampler != nullptr) {
		audio_resampler_destroy(m_mic_resampler);
		m_mic_resampler = nullptr;
	}
	// 释放扬声器回音消除音频转换器...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
	// 释放回音消除音频转换器...
	if (m_echo_resampler != nullptr) {
		audio_resampler_destroy(m_echo_resampler);
		m_echo_resampler = nullptr;
	}
	// 释放AAC压缩原始PCM缓存空间...
	if (m_aac_frame_ptr != nullptr) {
		av_free(m_aac_frame_ptr);
		m_aac_frame_ptr = NULL;
	}
	// 释放回音消除互斥对象...
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

// 处理右侧播放线程已经停止通知...
BOOL CWebrtcAEC::onUdpRecvThreadStop()
{
	// 这时一定没有扬声器对象...
	if (App()->GetAudioHorn())
		return false;
	ASSERT(!App()->GetAudioHorn());
	// 释放回音消除对象...
	if (m_hAEC != NULL) {
		WebRtcAec_Free(m_hAEC);
		m_hAEC = NULL;
	}
	// 初始化回音消除管理器 => Webrtc
	m_hAEC = WebRtcAec_Create();
	if (m_hAEC == NULL)
		return false;
	// 设置不确定延时，自动对齐波形 => 必须在初始化之前设置...
	Aec * lpAEC = (Aec*)m_hAEC;
	WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
	WebRtcAec_enable_extended_filter(lpAEC->aec, true);
	// 初始化回音消除对象失败，直接返回...
	if (WebRtcAec_Init(m_hAEC, m_out_sample_rate, m_out_sample_rate) != 0)
		return false;
	// 释放扬声器回音消除音频转换器...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
	// 注意：缓存必须清理，否则会影响新的回音消除...
	// 重新初始化扬声器环形缓存队列对象...
	circlebuf_free(&m_circle_horn);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	// 重建回音消除对象成功...
	return true;
}

BOOL CWebrtcAEC::InitWebrtc(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum, int nOutBitrateAAC)
{
	// 根据索引获取采样率...
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
	// 保存AAC压缩输出码流...
	m_aac_out_bitrate = nOutBitrateAAC;
	// 保存采样率索引和声道...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	// 保存输出采样率和输出声道...
	m_out_sample_rate = nOutSampleRate;
	m_out_channel_num = nOutChannelNum;

	// 初始化回音消除信号量...
	if (os_sem_init(&m_lpAECSem, 0) != 0)
		return false;
	// 信号量初始化失败，直接返回...
	if (m_lpAECSem == NULL)
		return false;
	// 初始化回音消除管理器 => Webrtc
	m_hAEC = WebRtcAec_Create();
	if (m_hAEC == NULL)
		return false;
	// 设置不确定延时，自动对齐波形 => 必须在初始化之前设置...
	Aec * lpAEC = (Aec*)m_hAEC;
	WebRtcAec_enable_delay_agnostic(lpAEC->aec, true);
	WebRtcAec_enable_extended_filter(lpAEC->aec, true);
	// 初始化回音消除对象失败，直接返回...
	if (WebRtcAec_Init(m_hAEC, m_out_sample_rate, m_out_sample_rate) != 0)
		return false;
	// 每次回音消除处理样本数，需要乘以声道数...
	m_nWebrtcNN = DEF_WEBRTC_AEC_NN * m_out_channel_num;
	// 计算每次处理设定样本数占用的毫秒数...
	m_nWebrtcMS = m_nWebrtcNN / ((m_out_sample_rate / 1000) * m_out_channel_num);
	// 根据每次处理样本数据分配回音消除需要用到的数据空间 => 数据是short格式...
	// 注意：这里使用obs的内存管理接口，可以进行泄漏跟踪 => bzalloc => bfree
	m_lpMicBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_lpHornBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_lpEchoBufNN = (short*)bzalloc(m_nWebrtcNN * sizeof(short));
	m_pDMicBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));
	m_pDHornBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));
	m_pDEchoBufNN = (float*)bzalloc(m_nWebrtcNN * sizeof(float));

	// 创建针对麦克风的降噪模块对象 => short格式...
	int err_code = 0;
	m_lpNsxInst = WebRtcNsx_Create();
	// 初始化降噪对象，使用16k频率...
	if (m_lpNsxInst != NULL) {
		err_code = WebRtcNsx_Init(m_lpNsxInst, m_out_sample_rate);
	}
	// 设定降噪程度 => 0: Mild, 1: Medium , 2: Aggressive
	if (m_lpNsxInst != NULL && err_code == 0) {
		err_code = WebRtcNsx_set_policy(m_lpNsxInst, 2);
	}

	// 初始化ffmpeg...
	av_register_all();

	// 设置解码器、压缩器需要的共同参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 设置压缩器需要转换的PCM音频数据格式 => 回音数据 转 AAC压缩前数据...
	// (Decoder)AV_SAMPLE_FMT_FLTP => (EchoCancel)AV_SAMPLE_FMT_S16 => (Encoder)AV_SAMPLE_FMT_FLTP
	AVSampleFormat echo_sample_fmt = AV_SAMPLE_FMT_S16;
	AVSampleFormat aac_sample_fmt = AV_SAMPLE_FMT_FLTP;

	/////////////////////////////////////////////////////////////////
	// 初始化AAC解码器对象...
	/////////////////////////////////////////////////////////////////
	// 查找需要的解码器和相关容器、解析器...
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// 打开获取到的解码器...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) != 0)
		return false;
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);

	/////////////////////////////////////////////////////////////////
	// 初始化AAC压缩器对象...
	/////////////////////////////////////////////////////////////////
	// 设置压缩器需要转换的PCM音频数据格式 => 回音数据 转 AAC压缩前数据...
	AVSampleFormat enc_in_sample_fmt = echo_sample_fmt;
	AVSampleFormat enc_out_sample_fmt = aac_sample_fmt;
	// 设置压缩器需要转换的声道数 => 声道数不变...
	int enc_in_channel_num = m_out_channel_num;
	int enc_out_channel_num = m_out_channel_num;
	// 设置压缩器需要转换的输入输出采样率 => 采样率不变...
	int enc_in_sample_rate = m_out_sample_rate;
	int enc_out_sample_rate = m_out_sample_rate;
	// 查找需要的压缩器和相关容器、解析器...
	m_lpEncCodec = avcodec_find_encoder(src_codec_id);
	m_lpEncContext = avcodec_alloc_context3(m_lpEncCodec);
	// 配置压缩器的相关参数信息...
	m_lpEncContext->codec_id = src_codec_id;
	m_lpEncContext->codec_type = AVMEDIA_TYPE_AUDIO;
	m_lpEncContext->sample_fmt = enc_out_sample_fmt;
	m_lpEncContext->sample_rate = enc_out_sample_rate;
	m_lpEncContext->channels = enc_out_channel_num;
	m_lpEncContext->channel_layout = av_get_default_channel_layout(enc_out_channel_num);
	m_lpEncContext->bit_rate = m_aac_out_bitrate;
	// 打开获取到的压缩器对象...
	int errCode = avcodec_open2(m_lpEncContext, m_lpEncCodec, NULL);
	if (errCode < 0) {
		char errBuf[MAX_PATH] = { 0 };
		av_strerror(errCode, errBuf, MAX_PATH);
		blog(LOG_INFO, "[AAC-Encoder] Error => %s", errBuf);
		return false;
	}
	// AAC压缩器每次需要处理的采样数据个数 => 压缩器默认分配的采样个数 => 1024
	m_aac_nb_samples = m_lpEncContext->frame_size;
	int enc_in_nb_samples = m_aac_nb_samples;
	int enc_out_nb_samples = av_rescale_rnd(enc_in_nb_samples, enc_out_sample_rate, enc_in_sample_rate, AV_ROUND_UP);
	// 计算AAC压缩器需要的PCM数据帧的字节数，并预先分配缓存...
	m_aac_frame_size = av_samples_get_buffer_size(NULL, enc_out_channel_num, enc_out_nb_samples, enc_out_sample_fmt, 1);
	m_aac_frame_ptr = (uint8_t *)av_malloc(m_aac_frame_size + 1);
	// 准备一个全局的压缩结构体 => 压缩数据帧是相互关联的...
	m_lpEncFrame = av_frame_alloc();
	ASSERT(m_lpEncFrame != NULL);

	// 设置麦克风输入的音频格式...
	resample_info micInfo = { 0 };
	micInfo.samples_per_sec = m_in_sample_rate;
	micInfo.speakers = convert_speaker_layout(nInChannelNum);
	micInfo.format = convert_sample_format(m_lpDecContext->sample_fmt);
	// 设置AAC压缩需要的音频格式...
	m_aac_sample_info.samples_per_sec = nOutSampleRate;
	m_aac_sample_info.speakers = convert_speaker_layout(nOutChannelNum);
	m_aac_sample_info.format = convert_sample_format(aac_sample_fmt);
	// 设置回音消除之后的音频格式...
	m_echo_sample_info.samples_per_sec = nOutSampleRate;
	m_echo_sample_info.speakers = convert_speaker_layout(nOutChannelNum);
	m_echo_sample_info.format = convert_sample_format(echo_sample_fmt);
	// 创建麦克风原始数据的重采样对象，将麦克风采集到的数据样本转换成回音消除需要的样本格式...
	m_mic_resampler = audio_resampler_create(&m_echo_sample_info, &micInfo);
	// 创建AAC压缩原始数据的重采样对象，将回音消除采集到的数据样本转换成AAC压缩需要的样本格式...
	m_echo_resampler = audio_resampler_create(&m_aac_sample_info, &m_echo_sample_info);
	// 如果创建转换对象失败，直接返回...
	if (m_mic_resampler == NULL || m_echo_resampler == NULL) {
		blog(LOG_INFO, "Error => audio_resampler_create");
		return false;
	}

	// 启动回声消除线程...
	this->Start();

	return true;
}

void CWebrtcAEC::doSaveAudioPCM(void * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID)
{
	// 如果不能进行AEC采样的存盘工作，直接返回...
	if (!App()->IsSaveAECSample())
		return;
	// 注意：PCM数据必须用二进制方式打开文件...
	char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "D:/MP4/PCM/IPC_%d_%d_%d.pcm", nAudioRate, nAudioChannel, nDBCameraID);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// 打开文件成功，开始写入音频PCM数据内容...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}

// 由于换成了WSAPI的音频格式，就是标准的float格式...
BOOL CWebrtcAEC::PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里不能限制扬声器投递音频数据，只要对象有效就可以投递数据...
	// 注意：只有在线通道才能回音消除，也就是不可能发生只有扬声器数据而没有麦克风数据的情况...
	// 注意：这里要是限制投递，会造成麦克风与扬声器起始数据不是严格按照声卡的缓存时间对齐的问题...
	////////////////////////////////////////////////////////////////////////////////////////////
	// 如果扬声器的转换对象为空，需要进行转换器的生成...
	if (m_horn_resampler == nullptr) {
		resample_info hornInfo = { 0 };
		// 注意WSAPI扬声器音频是固定的float格式...
		hornInfo.samples_per_sec = nSampleRate;
		hornInfo.speakers = convert_speaker_layout(nChannelNum);
		hornInfo.format = convert_sample_format(AV_SAMPLE_FMT_FLT);
		// 创建扬声器原始数据的重采样对象，将扬声器采集到的数据样本转换成回音消除需要的样本格式...
		m_horn_resampler = audio_resampler_create(&m_echo_sample_info, &hornInfo);
		// 如果创建转换对象失败，直接返回...
		if (m_horn_resampler == NULL) {
			blog(LOG_INFO, "Error => audio_resampler_create");
			return false;
		}
		// 打印第一个扬声器投递的音频数据时间戳 => 打印当前时刻的麦克风缓存长度...
		blog(LOG_INFO, "== horn-aec first packet: %I64d, mic-size: %d, horn-size: %d ==", 
			 os_gettime_ns() / 1000000, m_circle_mic.size, m_circle_horn.size);
		// 必须在正式投递扬声器消除数据时，才进行标志设定，就能避免网络延迟对AEC的影响...
		App()->SetAudioHorn(true);
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	uint32_t  input_frames = nBufSize / (nChannelNum * sizeof(float));
	// 对输入的原始音频样本格式进行统一的格式转换，转换成功，放入环形队列 => 转换成short格式...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&lpBufData, input_frames)) {
		// 计算格式转换之后的数据内容长度，并将转换后的数据放入组块缓存当中...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		// 开启互斥，循环压入环形队列当中...
		pthread_mutex_lock(&m_AECMutex);
		circlebuf_push_back(&m_circle_horn, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_AECMutex);
		// 发起信号量变化通知，可以进行回音消除操作了...
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
	return true;
}

// 注意：无论扬声器是否有效，都要对麦克风数据进行处理...
BOOL CWebrtcAEC::PushMicFrame(FMS_FRAME & inFrame)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	// 如果解码对象为空，直接返回失败...
	if (m_lpDecContext == NULL || m_lpDecCodec == NULL || m_lpDecFrame == NULL)
		return false;
	// 如果麦克风音频转换器无效，直接返回...
	if (m_mic_resampler == NULL)
		return false;
	// 加上ADTS数据包头...
	string & inData = inFrame.strData;
	uint32_t inPTS = inFrame.dwSendTime;
	uint32_t inOffset = inFrame.dwRenderOffset;
	bool bIsKeyFrame = inFrame.is_keyframe;
	// 先加入ADTS头，再加入数据帧内容...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// 构造ADTS帧头...
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

	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// 先添加帧头数据，再添加帧内容信息...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// 计算当前帧的PTS，关键帧标志 => 音频流 => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	int got_picture = 0, nResult = 0;
	// 注意：这里解码后的格式是AV_SAMPLE_FMT_FLTP，需要进行格式转换...
	nResult = avcodec_decode_audio4(m_lpDecContext, m_lpDecFrame, &got_picture, &theNewPacket);
	// 如果没有解出完整数据包，打印信息，释放数据包...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
		av_free_packet(&theNewPacket);
		return false;
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// 对原始麦克风音频样本格式，转换成回音消除需要的样本格式，转换成功放入环形队列 => AV_SAMPLE_FMT_S16 => short格式...
	if (audio_resampler_resample(m_mic_resampler, output_data, &output_frames, &ts_offset, m_lpDecFrame->data, m_lpDecFrame->nb_samples)) {
		// 计算格式转换之后的数据内容长度，并将转换后的数据放入分块缓存当中 => 每个块10毫秒 => 每个块都有独立的PTS...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		// 开启互斥，循环压入环形队列当中 => 分块处理，每个块10毫秒，每个块都有独立的PTS...
		this->doPushToMic(theNewPacket.pts, output_data[0], cur_data_size);
		// 发起信号量变化通知，可以进行回音消除操作了...
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
	// 释放AVPacket数据包...
	av_free_packet(&theNewPacket);
	return true;
}

void CWebrtcAEC::doPushToMic(int64_t inPTS, uint8_t * lpData, int nSize)
{
	pthread_mutex_lock(&m_AECMutex);
	// 每个数据块的样本大小 => 10毫秒 => AV_SAMPLE_FMT_S16 => short类型...
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	// 计算上次分块残余占用的毫秒数 => 需要计算每毫秒占用的字节数...
	int remain_ms = m_strRemainMic.size() * m_nWebrtcMS / nNeedBufBytes;
	// 将剩余数据累加，并计算开始PTS的数值 => 向前减去剩余毫秒数...
	int64_t start_pts = inPTS - remain_ms;
	m_strRemainMic.append((char*)lpData, nSize);
	// 计算最终生成的数据块个数 => 10毫秒数据块的个数...
	int nBlockSize = m_strRemainMic.size() / nNeedBufBytes;
	const char * lpStartMic = m_strRemainMic.c_str();
	for (int i = 0; i < nBlockSize; ++i) {
		int64_t cur_pts = start_pts + i * m_nWebrtcMS;
		const char * cur_mic = lpStartMic + i * nNeedBufBytes;
		circlebuf_push_back(&m_circle_mic, &cur_pts, sizeof(int64_t));
		circlebuf_push_back(&m_circle_mic, cur_mic, nNeedBufBytes);
		//blog(LOG_INFO, "Mic-Block => PTS: %I64d, Size: %d", cur_pts, nNeedBufBytes);
	}
	// 删除已经存入环形队列的音频数据内容，剩余的是不足10毫秒数据...
	m_strRemainMic.erase(0, nBlockSize * nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
}

void CWebrtcAEC::Entry()
{
	// 阻塞等待信号量发生变化...
	while (os_sem_wait(m_lpAECSem) == 0) {
		// 如果线程已经退出，中断循环...
		if (this->IsStopRequested())
			break;
		// 进行单独麦克风处理...
		this->doEchoMic();
		// 进行回声消除工作...
		this->doEchoCancel();
		// 进行AAC压缩打包...
		this->doEchoEncode();
	}
}

// 扬声器无效时才会执行...
void CWebrtcAEC::doEchoMic()
{
	// 如果扬声器已开启，不处理，直接返回...
	if (App()->GetAudioHorn())
		return;
	// 麦克风数据必须大于一个处理单元 => 样本数 * 每个样本占用字节数 => AV_SAMPLE_FMT_S16...
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	if (m_circle_mic.size < (nNeedBufBytes + sizeof(int64_t)))
		return;
	int64_t frame_pts_ms = 0;
	// 从环形队列当中读取一块数据 => 持续10毫秒...
	pthread_mutex_lock(&m_AECMutex);
	circlebuf_pop_front(&m_circle_mic, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
	// 对麦克风数据进行降噪处理 => short格式...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Process(m_lpNsxInst, &m_lpMicBufNN, m_out_channel_num, &m_lpEchoBufNN);
		memcpy(m_lpMicBufNN, m_lpEchoBufNN, nNeedBufBytes);
	}
	// 对数据进行直接转换...
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nWebrtcNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// 直接对解码后的麦克风音频样本格式，转换成AAC压缩器需要的音频样本格式，转换成功放入环形队列，AV_SAMPLE_FMT_S16 => AV_SAMPLE_FMT_FLTP...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpMicBufNN, input_frames)) {
		// 对降噪后的数据放入AAC压缩打包处理环形队列当中，进行下一步的压缩打包投递处理 => 注意，这里不需要用互斥对象...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
		//blog(LOG_INFO, "Echo-Block => PTS: %I64d, Size: %d", frame_pts_ms, cur_data_size);
	}
	// 如果麦克风环形队列里面还有足够的数据块需要进行处理，发起信号量变化事件...
	if (m_circle_mic.size > nNeedBufBytes) {
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
}

// 扬声器有效时，才会执行...
void CWebrtcAEC::doEchoCancel()
{
	// 如果扬声器没有开启，不处理，直接返回...
	if (!App()->GetAudioHorn())
		return;
	// 麦克风和扬声器都必须大于一个处理单元 => 样本数 * 每个样本占用字节数 => AV_SAMPLE_FMT_S16
	int nNeedBufBytes = m_nWebrtcNN * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	if (m_circle_mic.size < (nNeedBufBytes + sizeof(int64_t)) || m_circle_horn.size < nNeedBufBytes)
		return;
	int err_code = 0;
	int64_t frame_pts_ms = 0;
	// 从麦克风和扬声器环形队列中同时读取数据...
	pthread_mutex_lock(&m_AECMutex);
	circlebuf_pop_front(&m_circle_mic, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	circlebuf_pop_front(&m_circle_horn, m_lpHornBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_AECMutex);
	// 将short数据格式转换成float数据格式...
	for (int i = 0; i < m_nWebrtcNN; ++i) {
		m_pDMicBufNN[i] = (float)m_lpMicBufNN[i];
		m_pDHornBufNN[i] = (float)m_lpHornBufNN[i];
	}
	// 注意：使用自动计算延时模式，可以将msInSndCardBuf参数设置为0...
	// 先将扬声器数据进行远端投递，再投递麦克风数据进行回音消除 => 投递样本个数...
	err_code = WebRtcAec_BufferFarend(m_hAEC, m_pDHornBufNN, m_nWebrtcNN);
	err_code = WebRtcAec_Process(m_hAEC, &m_pDMicBufNN, m_out_channel_num, &m_pDEchoBufNN, m_nWebrtcNN, 0, 0);
	// 将float数据格式转换成short数据格式...
	for (int i = 0; i < m_nWebrtcNN; ++i) {
		m_lpEchoBufNN[i] = (short)m_pDEchoBufNN[i];
	}
	// 将麦克风的PCM数据进行存盘处理 => 必须用二进制方式打开文件...
	this->doSaveAudioPCM((void*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0, m_lpViewCamera->GetDBCameraID());
	// 将扬声器的PCM数据进行存盘处理 => 必须用二进制方式打开文件...
	this->doSaveAudioPCM((void*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1, m_lpViewCamera->GetDBCameraID());
	// 对回音消除后的数据进行存盘处理 => 必须用二进制方式打开文件...
	this->doSaveAudioPCM((void*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2, m_lpViewCamera->GetDBCameraID());
	// 对回音消除后的数据进行降噪处理 => short格式...
	if (m_lpNsxInst != NULL) {
		WebRtcNsx_Process(m_lpNsxInst, &m_lpEchoBufNN, m_out_channel_num, &m_lpMicBufNN);
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// 对降噪后的回音消除数据进行存盘处理 => 必须用二进制方式打开文件...
	this->doSaveAudioPCM((void*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 3, m_lpViewCamera->GetDBCameraID());
	// 发生错误，打印错误信息...
	if (err_code != 0) {
		blog(LOG_INFO, "== err: %d, mic_buf: %d, horn_buf: %d ==", err_code, m_circle_mic.size, m_circle_horn.size);
	}

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nWebrtcNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// 对回音消除后的音频样本格式，转换成AAC压缩器需要的音频样本格式，转换成功放入环形队列...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpEchoBufNN, input_frames)) {
		// 对回音消除后的数据放入AAC压缩打包处理环形队列当中，进行下一步的压缩打包投递处理 => 注意，这里不需要用互斥对象...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
	}

	// 如果麦克风和扬声器环形队列里面还有足够的数据块需要进行回音消除，发起信号量变化事件...
	if (m_circle_mic.size > nNeedBufBytes && m_circle_horn.size >= nNeedBufBytes) {
		((m_lpAECSem != NULL) ? os_sem_post(m_lpAECSem) : NULL);
	}
}

void CWebrtcAEC::doEchoEncode()
{
	// 计算 m_nWebrtcNN 对应的实际样本个数 => m_nWebrtcNN包含声道个数 => 由于采样率没有变化，因此样本个数也不会有变化...
	int nb_samples = av_rescale_rnd(m_nWebrtcNN, m_aac_sample_info.samples_per_sec, m_echo_sample_info.samples_per_sec, AV_ROUND_UP);
	// 回音缓存数据必须大于一个处理单元 => 样本数 * 每个样本占用字节数 => AV_SAMPLE_FMT_FLTP
	int nPerSampleByte = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP);
	int nNeedBufBytes = nb_samples * nPerSampleByte;
	if (m_circle_echo.size < (nNeedBufBytes + sizeof(int64_t)))
		return;
	int64_t frame_pts_ms = 0;
	int cur_frame_size = nNeedBufBytes;
	// 从回音消除后的队列中取出一个10毫秒音频数据...
	circlebuf_pop_front(&m_circle_echo, &frame_pts_ms, sizeof(int64_t));
	circlebuf_pop_front(&m_circle_echo, m_aac_frame_ptr, cur_frame_size);
	ASSERT(cur_frame_size < m_aac_frame_size);
	// 计算当前剩余缓存数据可以持续播放的毫秒数 => 剩余总字节数 / (每毫秒样本数 * 每样本字节数)
	int64_t aac_cur_pts = frame_pts_ms - m_strRemainEcho.size() / ((m_out_sample_rate / 1000) * m_out_channel_num * nPerSampleByte);
	// 将获取到的的音频数据追加到剩余缓存数据的末尾...
	m_strRemainEcho.append((char*)m_aac_frame_ptr, cur_frame_size);
	// 如果需要的原始音频缓存不够，返回等待...
	if (m_strRemainEcho.size() < m_aac_frame_size)
		return;
	// 获取待压缩音频数据的头指针位置...
	ASSERT(m_strRemainEcho.size() >= m_aac_frame_size);
	uint8_t * lpEchoData = (uint8_t*)m_strRemainEcho.c_str();
	// 累加AAC已压缩帧计数器...
	++m_aac_frame_count;
	// 为AVFrame准备相关的类型格式...
	m_lpEncFrame->nb_samples = m_lpEncContext->frame_size;
	m_lpEncFrame->format = m_lpEncContext->sample_fmt;
	m_lpEncFrame->pts = aac_cur_pts;
	// 使用原始的PCM数据填充AVFrame，准备开始压缩...
	int nFillSize = avcodec_fill_audio_frame(m_lpEncFrame, m_lpEncContext->channels, m_lpEncContext->sample_fmt, lpEchoData, m_aac_frame_size, 1);
	// 删除已经压缩的音频数据内容，剩余的是不足10毫秒数据...
	if (nFillSize < 0) {
		m_strRemainEcho.erase(0, m_aac_frame_size);
		return;
	}
	int got_frame = 0;
	AVPacket pkt = { 0 };
	// 采用新的压缩函数，将PCM数据压缩成AAC数据...
	int nResult = avcodec_encode_audio2(m_lpEncContext, &pkt, m_lpEncFrame, &got_frame);
	// 压缩失败或没有得到完整AAC数据帧，删除已经压缩的音频数据内容...
	if (nResult < 0 || !got_frame) {
		m_strRemainEcho.erase(0, m_aac_frame_size);
		return;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// 计算当前AAC压缩后的数据帧的PTS => 这种时间戳的计算方式存在巨大隐患...
	// m_aac_frame_count 并不一定精确，时间长了会造成严重的延迟问题...
	// 新的模型，始终采用IPC投递过来的原始时间戳，跟视频时间戳保持一致...
	// int64_t aac_cur_pts = m_mic_aac_ms_zero + m_aac_frame_count * aac_frame_duration;
	///////////////////////////////////////////////////////////////////////////////////

	// 打印AAC压缩后的结果信息，投递PTS不建议使用 pkt.pts，因为，刚开始的时间戳可能不精确...
	//blog(LOG_INFO, "Encode-AAC => inPTS: %I64d, inSize: %d, outPTS: %I64d, outSize: %d", aac_cur_pts, m_aac_frame_size, pkt.pts, pkt.size);

	// 构造返回需要的数据帧...
	FMS_FRAME theAACFrame;
	theAACFrame.strData.assign((char*)pkt.data, pkt.size);
	theAACFrame.typeFlvTag = FLV_TAG_TYPE_AUDIO;
	theAACFrame.dwSendTime = aac_cur_pts;
	theAACFrame.dwRenderOffset = 0;
	theAACFrame.is_keyframe = true;
	// 对压缩后的AAC数据帧进行投递处理...
	m_lpViewCamera->doPushAudioAEC(theAACFrame);
	// 释放ffmpeg内部分配的资源...
	av_packet_unref(&pkt);
	// 删除已经压缩的音频数据内容，剩余的是不足10毫秒数据...
	m_strRemainEcho.erase(0, m_aac_frame_size);
}