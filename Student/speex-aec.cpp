
#include "speex-aec.h"
#include "BitWritter.h"
#include "push-thread.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
};

CSpeexAEC::CSpeexAEC(CPushThread * lpPushThread)
  : m_lpPushThread(lpPushThread)
  , m_lpADecoder(nullptr)
  , m_lpACodec(nullptr)
  , m_lpDFrame(nullptr)
{
	m_audio_sample_rate = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
	m_au_convert_ctx = NULL;
	m_out_buffer_ptr = NULL;
	m_out_buffer_size = 0;
}

CSpeexAEC::~CSpeexAEC()
{
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
	if (m_au_convert_ctx != nullptr) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// 关闭音频缓冲区...
	if (m_out_buffer_ptr != NULL) {
		av_free(m_out_buffer_ptr);
		m_out_buffer_ptr = NULL;
	}
}

BOOL CSpeexAEC::InitSpeex(int nRateIndex, int nChannelNum)
{
	// 根据索引获取采样率...
	switch (nRateIndex)
	{
	case 0x03: m_audio_sample_rate = 48000; break;
	case 0x04: m_audio_sample_rate = 44100; break;
	case 0x05: m_audio_sample_rate = 32000; break;
	case 0x06: m_audio_sample_rate = 24000; break;
	case 0x07: m_audio_sample_rate = 22050; break;
	case 0x08: m_audio_sample_rate = 16000; break;
	case 0x09: m_audio_sample_rate = 12000; break;
	case 0x0a: m_audio_sample_rate = 11025; break;
	case 0x0b: m_audio_sample_rate = 8000; break;
	default:   m_audio_sample_rate = 48000; break;
	}
	// 保存采样率索引和声道...
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;
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
	// 输入声道和输出声道是一样的...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输出音频采样格式...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// 设置输入输出采样率 => 不变...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;
	// 获取音频解码后输出的缓冲区大小...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer_ptr = (uint8_t *)av_malloc(m_out_buffer_size);
	// 分配并初始化转换器...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpADecoder->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);
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

BOOL CSpeexAEC::PushFrame(FMS_FRAME & inFrame)
{
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
	put_bits(&pb, 4, m_audio_rate_index);
	put_bits(&pb, 1, 0);        /* private_bit */
	put_bits(&pb, 3, m_audio_channel_num); /* channel_configuration */
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
	do {
		// 如果没有解出完整数据包，打印信息...
		if (nResult < 0 || !got_picture) {
			blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", theNewPacket.pts, nResult, theNewPacket.size);
			break;
		}
		// 对解码后的音频进行类型转换 => AV_SAMPLE_FMT_S16
		memset(m_out_buffer_ptr, 0, m_out_buffer_size);
		nResult = swr_convert(m_au_convert_ctx, &m_out_buffer_ptr, m_out_buffer_size, (const uint8_t **)m_lpDFrame->data, m_lpDFrame->nb_samples);
		// 格式转换成功，将数据存入PCM文件当中 => 必须用二进制方式打开文件...
		if (nResult > 0) {
			this->doSaveAudioPCM(m_out_buffer_ptr, m_out_buffer_size, m_audio_sample_rate, m_audio_channel_num, m_lpPushThread->GetDBCameraID());
		}
	} while (false);
	// 释放AVPacket数据包...
	av_free_packet(&theNewPacket);
	return true;
}
