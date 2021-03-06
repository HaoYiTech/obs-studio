
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
	// 初始化麦克风和扬声器的样本转换器...
	m_mic_resampler = NULL;
	m_horn_resampler = NULL;
	m_echo_resampler = NULL;
	// 设置AAC每帧样本数...
	m_aac_nb_samples = 1024;
	m_aac_frame_count = 0;
	m_aac_out_bitrate = 0;
	m_aac_frame_size = 0;
	m_aac_frame_ptr = NULL;
	// 设置默认参数值...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	// 设置默认输出声道、输出采样率...
	m_out_channel_num = 1;
	m_out_sample_rate = 8000;
	// 设置默认的0点时刻点...
	m_mic_pts_ms_zero = -1;
	m_mic_sys_ns_zero = -1;
	m_horn_sys_ns_zero = -1;
	// 初始化各个环形队列对象...
	circlebuf_init(&m_circle_mic);
	circlebuf_init(&m_circle_horn);
	circlebuf_init(&m_circle_echo);
	circlebuf_reserve(&m_circle_mic, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_horn, DEF_CIRCLE_SIZE / 4);
	circlebuf_reserve(&m_circle_echo, DEF_CIRCLE_SIZE / 4);
	// 初始化回音消除互斥对象...
	pthread_mutex_init_value(&m_SpeexMutex);
}

CSpeexAEC::~CSpeexAEC()
{
	// 先对线程进行退出标志设定，避免再次进入信号量阻塞等待...
	this->SendStopRequest();
	// 发起信号量变化事件，迫使线程主动退出...
	((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 释放解码结构体...
	if (m_lpDeFrame != NULL) {
		av_frame_free(&m_lpDeFrame);
		m_lpDeFrame = NULL;
	}
	// 释放解码器对象...
	if (m_lpDeContext != nullptr) {
		avcodec_close(m_lpDeContext);
		av_free(m_lpDeContext);
		m_lpDeContext = NULL;
	}
	// 释放压缩结构体...
	if (m_lpEnFrame != NULL) {
		av_frame_free(&m_lpEnFrame);
		m_lpEnFrame = NULL;
	}
	// 释放压缩器对象...
	if (m_lpEnContext != nullptr) {
		avcodec_close(m_lpEnContext);
		av_free(m_lpEnContext);
		m_lpEnContext = NULL;
	}
	// 释放回声消除相关对象...
	if (m_lpSpeexEcho != NULL) {
		speex_echo_state_destroy(m_lpSpeexEcho);
		m_lpSpeexEcho = NULL;
	}
	if (m_lpSpeexDen != NULL) {
		speex_preprocess_state_destroy(m_lpSpeexDen);
		m_lpSpeexDen = NULL;
	}
	// 释放回声消除信号量...
	if (m_lpSpeexSem != NULL) {
		os_sem_destroy(m_lpSpeexSem);
		m_lpSpeexSem = NULL;
	}
	// 释放音频环形队列...
	circlebuf_free(&m_circle_mic);
	circlebuf_free(&m_circle_horn);
	circlebuf_free(&m_circle_echo);
	// 释放回音消除使用到的缓存空间...
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
	// 扬声器设定的播放延迟时间(毫秒)...
	m_horn_delay_ms = nHornDelayMS;
	// 计算每次处理设定毫秒数占用的样本数(默认16毫秒)
	m_nSpeexNN = nSpeexFrameMS * (m_out_sample_rate / 1000) * m_out_channel_num;
	// 计算尾音为设定毫秒占用的样本数(默认400毫秒) => 允许的同步落差，越大，计算量越大，消除效果越好...
	m_nSpeexTAIL = nSpeexFilterMS * (m_out_sample_rate / 1000) * m_out_channel_num;
	// 根据计算结果分配回音消除需要用到的数据空间...
	m_lpMicBufNN = new short[m_nSpeexNN];
	m_lpHornBufNN = new short[m_nSpeexNN];
	m_lpEchoBufNN = new short[m_nSpeexNN];
	// 初始化回音消除信号量...
	if (os_sem_init(&m_lpSpeexSem, 0) != 0)
		return false;
	// 信号量初始化失败，直接返回...
	if (m_lpSpeexSem == NULL)
		return false;
	// 初始化回音消除管理器 => speex
 	m_lpSpeexEcho = speex_echo_state_init(m_nSpeexNN, m_nSpeexTAIL);
	m_lpSpeexDen = speex_preprocess_state_init(m_nSpeexNN, m_out_sample_rate);
	speex_echo_ctl(m_lpSpeexEcho, SPEEX_ECHO_SET_SAMPLING_RATE, &m_out_sample_rate);
	speex_preprocess_ctl(m_lpSpeexDen, SPEEX_PREPROCESS_SET_ECHO_STATE, m_lpSpeexEcho);

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
	m_lpDeCodec = avcodec_find_decoder(src_codec_id);
	m_lpDeContext = avcodec_alloc_context3(m_lpDeCodec);
	// 打开获取到的解码器...
	if (avcodec_open2(m_lpDeContext, m_lpDeCodec, NULL) != 0)
		return false;
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDeFrame = av_frame_alloc();
	ASSERT(m_lpDeFrame != NULL);

	/*// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	// 注意：还没有开始转换，swr_get_delay()返回0，out_nb_samples是正常的样本数，不会变...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + m_aac_nb_samples;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// 计算输出每帧持续毫秒数(与声道无关)，每帧字节数(与声道有关) => 只需要计算一次就够了...
	int out_frame_duration = out_nb_samples * 1000 / out_sample_rate;
	int out_frame_bytes = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);*/

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
	// 计算输入输出的音频采样个数...
	int enc_in_nb_samples = m_aac_nb_samples;
	int enc_out_nb_samples = av_rescale_rnd(enc_in_nb_samples, enc_out_sample_rate, enc_in_sample_rate, AV_ROUND_UP);
	// 计算AAC压缩器需要的PCM数据帧的字节数，并预先分配缓存...
	m_aac_frame_size = av_samples_get_buffer_size(NULL, enc_out_channel_num, enc_out_nb_samples, enc_out_sample_fmt, 1);
	m_aac_frame_ptr = (uint8_t *)av_malloc(m_aac_frame_size + 1);
	// 查找需要的压缩器和相关容器、解析器...
	m_lpEnCodec = avcodec_find_encoder(src_codec_id);
	m_lpEnContext = avcodec_alloc_context3(m_lpEnCodec);
	// 配置压缩器的相关参数信息...
	m_lpEnContext->codec_id = src_codec_id;
	m_lpEnContext->codec_type = AVMEDIA_TYPE_AUDIO;
	m_lpEnContext->sample_fmt = enc_out_sample_fmt;
	m_lpEnContext->sample_rate = enc_out_sample_rate;
	m_lpEnContext->channels = enc_out_channel_num;
	m_lpEnContext->channel_layout = av_get_default_channel_layout(enc_out_channel_num);
	m_lpEnContext->bit_rate = m_aac_out_bitrate;
	// 打开获取到的压缩器对象...
	int errCode = avcodec_open2(m_lpEnContext, m_lpEnCodec, NULL);
	if (errCode < 0) {
		char errBuf[MAX_PATH] = { 0 };
		av_strerror(errCode, errBuf, MAX_PATH);
		blog(LOG_INFO, "[AAC-Encoder] Error => %s", errBuf);
		return false;
	}
	// 准备一个全局的压缩结构体 => 压缩数据帧是相互关联的...
	m_lpEnFrame = av_frame_alloc();
	ASSERT(m_lpEnFrame != NULL);

	// 设置麦克风输入的音频格式...
	resample_info micInfo = { 0 };
	micInfo.samples_per_sec = m_in_sample_rate;
	micInfo.speakers = convert_speaker_layout(nInChannelNum);
	micInfo.format = convert_sample_format(m_lpDeContext->sample_fmt);
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

	// 启动回声消除线程...
	this->Start();
	return true;
}

void CSpeexAEC::doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID)
{
	// 注意：PCM数据必须用二进制方式打开文件...
	char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "F:/MP4/PCM/IPC_%d_%d_%d.pcm", nAudioRate, nAudioChannel, nDBCameraID);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// 打开文件成功，开始写入音频PCM数据内容...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}

// 由于换成了WSAPI的音频格式，就是标准的float格式...
BOOL CSpeexAEC::PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	// 如果捕捉设备还没有初始化完毕，不能进行回音消除数据填充...
	if (m_mic_resampler == nullptr)
		return false;
	// 如果扬声器的转换对象为空，需要进行转换器的生成...
	if (m_horn_resampler == nullptr) {
		resample_info hornInfo = { 0 };
		// 注意WSAPI扬声器音频是固定的float格式...
		hornInfo.samples_per_sec = nSampleRate;
		hornInfo.speakers = convert_speaker_layout(nChannelNum);
		hornInfo.format = convert_sample_format(AV_SAMPLE_FMT_FLT);
		// 创建扬声器原始数据的重采样对象，将扬声器采集到的数据样本转换成回音消除需要的样本格式...
		m_horn_resampler = audio_resampler_create(&m_echo_sample_info, &hornInfo);
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	uint32_t  input_frames = nBufSize / (nChannelNum * sizeof(float));
	// 对输入的原始音频样本格式进行统一的格式转换，转换成功，放入环形队列...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&lpBufData, input_frames)) {
		// 注意：这里需要进行互斥保护，是多线程数据访问...
		pthread_mutex_lock(&m_SpeexMutex);
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_horn, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_SpeexMutex);
		// 发起信号量变化通知，可以进行回音消除操作了...
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}
	// 如果是第一个数据包，记录扬声器系统0点时刻...
	if (m_horn_sys_ns_zero < 0) {
		m_horn_sys_ns_zero = os_gettime_ns();
	}
	return true;

	// 如果是第一个数据包，记录扬声器系统0点时刻...
	/*pthread_mutex_lock(&m_SpeexMutex);
	if (m_horn_sys_ns_zero < 0) {
		// 每毫秒样本数 * 样本长度 * 延时毫秒数 * 声道数...
		int nDelayBytes = (m_out_sample_rate / 1000) * sizeof(short) * m_horn_delay_ms * m_out_channel_num;
		circlebuf_push_back_zero(&m_circle_horn, nDelayBytes);
		m_horn_sys_ns_zero = os_gettime_ns();
	}
	// 将扬声器音频数据内容放入环形队列，只存储PCM数据内容...
	circlebuf_push_back(&m_circle_horn, lpBufData, nBufSize);
	pthread_mutex_unlock(&m_SpeexMutex);
	// 发起信号量变化通知，可以进行回音消除操作了...
	((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	return true;*/
}

BOOL CSpeexAEC::PushMicFrame(FMS_FRAME & inFrame)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	// 如果解码对象为空，直接返回失败...
	if (m_lpDeContext == NULL || m_lpDeCodec == NULL || m_lpDeFrame == NULL)
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
	// 注意：这里解码后的格式是4bit，需要转换成16bit，调用swr_convert
	nResult = avcodec_decode_audio4(m_lpDeContext, m_lpDeFrame, &got_picture, &theNewPacket);
	// 如果没有解出完整数据包，打印信息，释放数据包...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
		av_free_packet(&theNewPacket);
		return false;
	}

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// 对原始麦克风音频样本格式，转换成回音消除需要的样本格式，转换成功放入环形队列...
	if (audio_resampler_resample(m_mic_resampler, output_data, &output_frames, &ts_offset, m_lpDeFrame->data, m_lpDeFrame->nb_samples)) {
		pthread_mutex_lock(&m_SpeexMutex);
		// 计算回音消除之后的数据内容长度，并将转换后的数据放入环形队列当中...
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_mic, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_SpeexMutex);
		// 如果是第一个数据包，需要记录系统0点时刻...
		if (m_mic_sys_ns_zero < 0) {
			m_mic_sys_ns_zero = os_gettime_ns();
		}
		// 如果是第一个数据包，需要记录PTS的0点时刻...
		if (m_mic_pts_ms_zero < 0) {
			m_mic_pts_ms_zero = theNewPacket.dts;
		}
		// 发起信号量变化通知，可以进行回音消除操作了...
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}

	// 释放AVPacket数据包...
	av_free_packet(&theNewPacket);
	return true;
}

void CSpeexAEC::Entry()
{
	// 阻塞等待信号量发生变化...
	while (os_sem_wait(m_lpSpeexSem) == 0) {
		// 如果线程已经退出，中断循环...
		if (this->IsStopRequested())
			break;
		// 进行回声消除工作...
		this->doEchoCancel();
		// 进行AAC压缩打包...
		this->doEncodeAAC();
	}
}

void CSpeexAEC::doEchoCancel()
{
	// 注意：这里并不考虑扬声器数据是否足够，尽最大可能降低麦克风延时...
	// 麦克风数据必须大于一个处理单元 => 样本数 * 每个样本占用字节数...
	int nNeedBufBytes = m_nSpeexNN * sizeof(short);
	if (m_circle_mic.size < nNeedBufBytes)
		return;
	// 读取麦克风需要的处理样本内容 => 注意是读取字节数，不是样本数...
	pthread_mutex_lock(&m_SpeexMutex);
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	pthread_mutex_unlock(&m_SpeexMutex);
	// 如果扬声器数据足够，读取扬声器数据到待处理样本缓存当中..
	if (m_circle_horn.size >= nNeedBufBytes) {
		// 从扬声器环形队列当中读取数据到扬声器样本空间当中...
		pthread_mutex_lock(&m_SpeexMutex);
		circlebuf_pop_front(&m_circle_horn, m_lpHornBufNN, nNeedBufBytes);
		pthread_mutex_unlock(&m_SpeexMutex);
		// 麦克风和扬声器样本数据准备就绪，先进行回音消除，再进行一个额外处理...
		speex_echo_cancellation(m_lpSpeexEcho, m_lpMicBufNN, m_lpHornBufNN, m_lpEchoBufNN);
		speex_preprocess_run(m_lpSpeexDen, m_lpEchoBufNN);
		// 将扬声器的PCM数据进行存盘处理 => 必须用二进制方式打开文件...
		//this->doSaveAudioPCM((uint8_t*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1, m_lpViewCamera->GetDBCameraID());
	} else {
		// 扬声器数据不够，把麦克风样本数据直接当成回音消除后的样本数据...
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// 对回音消除后的数据进行存盘处理 => 必须用二进制方式打开文件...
	//this->doSaveAudioPCM((uint8_t*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0, m_lpViewCamera->GetDBCameraID());
	//this->doSaveAudioPCM((uint8_t*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2, m_lpViewCamera->GetDBCameraID());

	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint32_t  input_frames = m_nSpeexNN;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// 对回音消除后的音频样本格式，转换成AAC压缩器需要的音频样本格式，转换成功放入环形队列...
	if (audio_resampler_resample(m_echo_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&m_lpEchoBufNN, input_frames)) {
		// 对回音消除后的数据放入AAC压缩打包处理环形队列当中，进行下一步的压缩打包投递处理 => 注意，这里不需要用互斥对象...
		int cur_data_size = get_audio_size(m_aac_sample_info.format, m_aac_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_echo, output_data[0], cur_data_size);
	}

	// 如果麦克风环形队列里面还有足够的数据块需要进行回音消除，发起信号量变化事件...
	if (m_circle_mic.size >= nNeedBufBytes) {
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}
}

void CSpeexAEC::doEncodeAAC()
{
	// 回音消除后的环形队列缓存不够，直接返回...
	if (m_circle_echo.size < m_aac_frame_size)
		return;
	// 计算AAC单帧持续的毫秒数，便于累加时间戳...
	int aac_frame_duration = m_aac_nb_samples * 1000 / m_out_sample_rate;
	// 从环形队列中抽取AAC压缩器需要的数据内容 => 不需要互斥对象，都是顺序执行...
	circlebuf_pop_front(&m_circle_echo, m_aac_frame_ptr, m_aac_frame_size);
	// 调用AAC压缩器，进行音频的压缩处理...
	// 计算当前AAC压缩后的数据帧的PTS...
	int64_t aac_cur_pts = m_mic_pts_ms_zero + m_aac_frame_count * aac_frame_duration;
	// 累加AAC已压缩帧计数器...
	++m_aac_frame_count;
	// 为AVFrame准备相关的类型格式...
	m_lpEnFrame->nb_samples = m_lpEnContext->frame_size;
	m_lpEnFrame->format = m_lpEnContext->sample_fmt;
	m_lpEnFrame->pts = aac_cur_pts;
	// 使用原始的PCM数据填充AVFrame，准备开始压缩...
	int nFillSize = avcodec_fill_audio_frame(m_lpEnFrame, m_lpEnContext->channels, m_lpEnContext->sample_fmt, m_aac_frame_ptr, m_aac_frame_size, 1);
	if (nFillSize < 0)
		return;
	int got_frame = 0;
	AVPacket pkt = { 0 };
	// 采用新的压缩函数，将PCM数据压缩成AAC数据...
	int nResult = avcodec_encode_audio2(m_lpEnContext, &pkt, m_lpEnFrame, &got_frame);
	// 压缩失败或没有得到完整AAC数据帧，直接返回，AVPacket没有数据...
	if (nResult < 0 || !got_frame)
		return;
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
}