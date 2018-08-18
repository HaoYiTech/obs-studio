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
	CViewCamera     *   m_lpViewCamera;     // ͨ������...
	AVCodec         *   m_lpACodec;			// ������...
	AVFrame         *   m_lpDFrame;			// ����ṹ��...
	AVCodecContext  *   m_lpADecoder;		// ����������...
	SwrContext      *   m_au_convert_ctx;	// ��Ƶ��ʽת��
	uint8_t         *   m_out_buffer_ptr;	// ��֡����ռ�
	int                 m_out_buffer_size;	// ��֡�����С
};