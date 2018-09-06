#pragma once

#include "OSThread.h"
#include "HYDefine.h"
#include <util/threading.h>
#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>

struct AVFrame;
struct AVCodec;
struct SwrContext;
struct AVCodecContext;
struct SpeexEchoState_;
struct SpeexPreprocessState_;

class CViewCamera;
class CSpeexAEC : public OSThread
{
public:
	CSpeexAEC(CViewCamera * lpViewCamera);
	virtual ~CSpeexAEC();
	virtual void Entry();
public:
	BOOL    InitSpeex(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum,
                      int nOutBitrateAAC, int nHornDelayMS, int nSpeexFrameMS, int nSpeexFilterMS);
	BOOL    PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum);
	BOOL    PushMicFrame(FMS_FRAME & inFrame);
private:
	void    doEncodeAAC();
	void    doEchoCancel();
	void    doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID);
private:
	int                 m_in_rate_index;    // �����������
	int                 m_in_channel_num;   // ����������
	int                 m_in_sample_rate;   // ���������
	int                 m_out_channel_num;  // ���������
	int                 m_out_sample_rate;  // ���������

	int                 m_aac_out_bitrate;  // AACѹ���������
	int                 m_aac_frame_count;  // AAC��ѹ��֡������
	int                 m_aac_nb_samples;   // AAC����ÿ֡������
	int                 m_aac_frame_size;   // AAC����ÿ֡PCM����
	uint8_t         *   m_aac_frame_ptr;    // AACԭʼPCM�ռ�����

	resample_info       m_aac_sample_info;  // AAC�����Ҫ����Ƶ������ʽ
	resample_info       m_echo_sample_info; // ����������Ҫ����Ƶ������ʽ
	audio_resampler_t * m_mic_resampler;    // ��˷�ԭʼ��������ת���ɻ�������������ʽ => mic ת echo
	audio_resampler_t * m_horn_resampler;   // ������ԭʼ��������ת���ɻ�������������ʽ => horn ת echo
	audio_resampler_t * m_echo_resampler;   // ����ԭʼ��������ת����AACѹ��ԭʼ������ʽ => echo ת aac

	CViewCamera     *   m_lpViewCamera;     // ͨ������...
	AVCodec         *   m_lpDeCodec;		// ������...
	AVFrame         *   m_lpDeFrame;		// ����ṹ��...
	AVCodecContext  *   m_lpDeContext;		// ����������...
	AVCodec         *   m_lpEnCodec;		// ѹ����...
	AVFrame         *   m_lpEnFrame;		// ѹ���ṹ��...
	AVCodecContext  *   m_lpEnContext;		// ѹ��������...
	int                 m_nSpeexNN;         // ÿ�δ���������
	int                 m_nSpeexTAIL;       // β����������
	short           *   m_lpMicBufNN;       // ��˷�ԭʼ����
	short           *   m_lpHornBufNN;      // ������ԭʼ����
	short           *   m_lpEchoBufNN;      // ��������������
	circlebuf           m_circle_mic;		// PCM���ζ��� => ֻ�����˷��������Ƶ����
	circlebuf           m_circle_horn;      // PCM���ζ��� => ֻ�����������������Ƶ����
	circlebuf           m_circle_echo;      // PCM���ζ��� => ֻ��Ż�������֮�����Ƶ����
	int64_t             m_mic_pts_ms_zero;  // ��˷������һ��AAC����֡��PTS(����) => 0��ʱ�䣬����ѹ�����PTS���㣬Ĭ��-1
	int64_t             m_mic_sys_ns_zero;  // ��˷��ѹ��һ��AAC����֡���ϵͳʱ��(����) => 0��ʱ�䣬�������������ȶԣ�Ѱ�һ�������ʱ�̵㣬Ĭ��-1
	int64_t             m_horn_sys_ns_zero; // �����������һ��PCM���ݰ����ϵͳʱ��(����) => 0��ʱ�䣬��������˷�ȶԣ�Ѱ�һ�������ʱ�̵㣬Ĭ��-1
	int                 m_horn_delay_ms;    // �������趨��Ĭ�ϲ����ӳ�ʱ��(����)��SDL�ڲ�����ĺ����� => ʹ��WSAPI֮���趨Ϊ0����...
	os_sem_t        *   m_lpSpeexSem;       // ���������ź���
	pthread_mutex_t     m_SpeexMutex;       // ��������������
	SpeexEchoState_ *   m_lpSpeexEcho;      // ������������
	SpeexPreprocessState_ * m_lpSpeexDen;   // ������������
};