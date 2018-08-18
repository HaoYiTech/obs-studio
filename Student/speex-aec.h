#pragma once

#include "HYDefine.h"

struct AVFrame;
struct AVCodec;
struct SwrContext;
struct AVCodecContext;

class CViewCamera;
class CSpeexAEC
{
public:
	CSpeexAEC(CViewCamera * lpViewCamera);
	~CSpeexAEC();
public:
	BOOL    InitSpeex(int nRateIndex, int nChannelNum);
	BOOL    PushFrame(FMS_FRAME & inFrame);
private:
	void    doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID);
private:
	int                 m_audio_rate_index;
	int                 m_audio_channel_num;
	int                 m_audio_sample_rate;
	CViewCamera     *   m_lpViewCamera;     // 通道对象...
	AVCodec         *   m_lpACodec;			// 解码器...
	AVFrame         *   m_lpDFrame;			// 解码结构体...
	AVCodecContext  *   m_lpADecoder;		// 解码器描述...
	SwrContext      *   m_au_convert_ctx;	// 音频格式转换
	uint8_t         *   m_out_buffer_ptr;	// 单帧输出空间
	int                 m_out_buffer_size;	// 单帧输出大小
};