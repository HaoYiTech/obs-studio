
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
  , m_lpADecoder(nullptr)
  , m_lpACodec(nullptr)
  , m_lpDFrame(nullptr)
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
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	m_out_convert_ctx = NULL;
	m_max_buffer_ptr = NULL;
	m_max_buffer_size = 0;
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
	if (m_lpDFrame != NULL) {
		av_frame_free(&m_lpDFrame);
		m_lpDFrame = NULL;
	}
	// 释放解码器对象...
	if (m_lpADecoder != nullptr) {
		avcodec_close(m_lpADecoder);
		av_free(m_lpADecoder);
	}
	// 关闭音频转换对象...
	if (m_out_convert_ctx != nullptr) {
		swr_free(&m_out_convert_ctx);
		m_out_convert_ctx = NULL;
	}
	// 关闭单帧音频最大缓冲区...
	if (m_max_buffer_ptr != NULL) {
		av_free(m_max_buffer_ptr);
		m_max_buffer_ptr = NULL;
		m_max_buffer_size = 0;
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
	// 释放回音消除互斥对象...
	pthread_mutex_destroy(&m_SpeexMutex);
}

BOOL CSpeexAEC::InitSpeex(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum,
                          int nHornDelayMS, int nSpeexFrameMS, int nSpeexFilterMS)
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
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpACodec = avcodec_find_decoder(src_codec_id);
	m_lpADecoder = avcodec_alloc_context3(m_lpACodec);
	// 打开获取到的解码器...
	if (avcodec_open2(m_lpADecoder, m_lpACodec, NULL) != 0)
		return false;
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDFrame = av_frame_alloc();
	ASSERT(m_lpDFrame != NULL);
	// 输入声道和输出声道 => 转换成单声道...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输入输出的音频格式...
	AVSampleFormat in_sample_fmt = m_lpADecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// 设置输入输出采样率 => 转换成8K采样率...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// 分配并初始化转换器...
	m_out_convert_ctx = swr_alloc();
	m_out_convert_ctx = swr_alloc_set_opts(m_out_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, in_sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_out_convert_ctx);
	// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	// 注意：还没有开始转换，swr_get_delay()返回0，out_nb_samples是正常的样本数，不会变...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + 1024;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// 计算输出每帧持续毫秒数(与声道无关)，每帧字节数(与声道有关) => 只需要计算一次就够了...
	int out_frame_duration = out_nb_samples * 1000 / out_sample_rate;
	int out_frame_bytes = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
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

BOOL CSpeexAEC::PushHornPCM(void * lpBufData, int nBufSize)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	// 如果是第一个数据包，记录扬声器系统0点时刻...
	pthread_mutex_lock(&m_SpeexMutex);
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
	return true;
}

BOOL CSpeexAEC::PushMicFrame(FMS_FRAME & inFrame)
{
	// 如果线程已经退出，直接返回...
	if (this->IsStopRequested())
		return false;
	// 如果解码对象为空，直接返回失败...
	if (m_lpADecoder == NULL || m_lpACodec == NULL)
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
	nResult = avcodec_decode_audio4(m_lpADecoder, m_lpDFrame, &got_picture, &theNewPacket);
	// 如果没有解出完整数据包，打印信息，释放数据包...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
		av_free_packet(&theNewPacket);
		return false;
	}
	// 对解码后的音频进行格式转换 => 采样率、声道、数据格式...
	this->doConvertAudio(theNewPacket.dts, m_lpDFrame);
	// 释放AVPacket数据包...
	av_free_packet(&theNewPacket);
	return true;
}

void CSpeexAEC::doConvertAudio(int64_t in_pts_ms, AVFrame * lpDFrame)
{
	// 输入声道和输出声道 => 转换成单声道...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	// 输入输出的音频格式...
	AVSampleFormat in_sample_fmt = m_lpADecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// 设置输入输出采样率 => 转换成8K采样率...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	// 注意：已经开始转换，swr_get_delay()将返回上次转换遗留的样本数，因此，要准备比正常样本大的空间...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + lpDFrame->nb_samples;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// 获取音频解码后输出的缓冲区大小 => 分配新的空间，由于有上次转换遗留样本，需要的空间会有变化...
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	// 如果新需要的空间大于了当前单帧最大空间，需要重建空间...
	if (out_buffer_size > m_max_buffer_size) {
		// 删除之前分配的单帧最大空间...
		if (m_max_buffer_ptr != NULL) {
			av_free(m_max_buffer_ptr);
			m_max_buffer_ptr = NULL;
		}
		// 重新分配最新的单帧最大空间...
		m_max_buffer_size = out_buffer_size;
		m_max_buffer_ptr = (uint8_t *)av_malloc(out_buffer_size);
	}
	// 调用音频转换接口，转换音频数据 => 返回值很重要，它是实际获取的已转换的样本数 => 使用最大空间去接收转换后的数据内容...
	int nResult = swr_convert(m_out_convert_ctx, &m_max_buffer_ptr, m_max_buffer_size, (const uint8_t **)lpDFrame->data, lpDFrame->nb_samples);
	// 转换失败，直接返回 => 没有需要抽取的转换后数据内容...
	if (nResult <= 0) return;
	// 需要将转换后的样本数，转换成实际的缓存大小，这就能解释为啥，在SDL播放时，总是会出现数据无法播放完的问题，因为，给了太多数据...
	int cur_data_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, nResult, out_sample_fmt, 1);
	// 注意：格式统一成8K采样率，单声道 => 必须用二进制方式打开文件...
	//this->doSaveAudioPCM(m_max_buffer_ptr, cur_data_size, m_out_sample_rate, m_out_channel_num, m_lpViewCamera->GetDBCameraID());
	// 如果是第一个数据包，需要记录系统0点时刻...
	if (m_mic_sys_ns_zero < 0) {
		m_mic_sys_ns_zero = os_gettime_ns();
	}
	// 如果是第一个数据包，需要记录PTS的0点时刻...
	if (m_mic_pts_ms_zero < 0) {
		m_mic_pts_ms_zero = in_pts_ms;
	}
	// 将获取到缓存放入缓冲队列当中，只存储PCM数据 => 从最大单帧空间中抽取本次转换的有效数据内容...
	pthread_mutex_lock(&m_SpeexMutex);
	circlebuf_push_back(&m_circle_mic, m_max_buffer_ptr, cur_data_size);
	pthread_mutex_unlock(&m_SpeexMutex);
	// 最终不是通过删除麦克风数据，而是通过给扬声器缓存增加空数据的方式解决...
	// 计算每个AAC数据帧占用的持续时间(毫秒)，每个AAC数据帧使用1024个PCM样本；计算每毫秒占用字节数...
	/*int in_frame_duration = 1024 * 1000 / m_in_sample_rate;
	int in_bytes_per_ms = m_in_sample_rate / 1000 * sizeof(short);
	// 设定PCM数据指针和数据大小...
	uint8_t * lpPCMDataPtr = m_max_buffer_ptr;
	int nPCMDataSize = cur_data_size;
	// 人为的降低麦克风与扬声器的同步落差 => 注意移动PTS的0点时刻...
	if (m_mic_drop_bytes >= cur_data_size) {
		m_mic_drop_bytes -= cur_data_size;
		m_mic_pts_ms_zero += in_frame_duration;
	} else if (m_mic_drop_bytes > 0) {
		// 计算剩余的PCM数据占用的毫秒数，移动PCM数据指针...
		m_mic_pts_ms_zero += m_mic_drop_bytes / in_bytes_per_ms;
		nPCMDataSize -= m_mic_drop_bytes;
		lpPCMDataPtr += m_mic_drop_bytes;
		m_mic_drop_bytes = 0;
	}
	// 将获取到缓存放入缓冲队列当中，只存储PCM数据...
	if (m_mic_drop_bytes <= 0) {
		pthread_mutex_lock(&m_SpeexMutex);
		circlebuf_push_back(&m_circle_mic, lpPCMDataPtr, nPCMDataSize);
		pthread_mutex_unlock(&m_SpeexMutex);
	}*/
	//blog(LOG_INFO, "[Audio-%d] MicPCM => PTS: %I64d, Size: %d", m_lpViewCamera->GetDBCameraID(), in_pts_ms, cur_data_size);
	// 发起信号量变化通知，可以进行回音消除操作了...
	((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
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
		this->doSaveAudioPCM((uint8_t*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1, m_lpViewCamera->GetDBCameraID());
	} else {
		// 扬声器数据不够，把麦克风样本数据直接当成回音消除后的样本数据...
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// 对回音消除后的数据进行存盘处理 => 必须用二进制方式打开文件...
	this->doSaveAudioPCM((uint8_t*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0, m_lpViewCamera->GetDBCameraID());
	this->doSaveAudioPCM((uint8_t*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2, m_lpViewCamera->GetDBCameraID());
	// 对回音消除后的数据放入AAC压缩打包处理环形队列当中，进行下一步的压缩打包投递处理 => 注意，这里不需要用互斥对象...
	//circlebuf_push_back(&m_circle_echo, m_lpEchoBufNN, nNeedBufBytes);
	// 如果麦克风环形队列里面还有足够的数据块需要进行回音消除，发起信号量变化事件...
	if (m_circle_mic.size >= nNeedBufBytes) {
		((m_lpSpeexSem != NULL) ? os_sem_post(m_lpSpeexSem) : NULL);
	}
}

void CSpeexAEC::doEncodeAAC()
{
}