#pragma once

#include "OSThread.h"
#include "HYDefine.h"
#include <util/threading.h>
#include <media-io/audio-resampler.h>
#include "noise_suppression_x.h"

struct AVFrame;
struct AVCodec;
struct AVCodecContext;

class CViewCamera;
class CWebrtcAEC : public OSThread
{
public:
	CWebrtcAEC(CViewCamera * lpViewCamera);
	virtual ~CWebrtcAEC();
	virtual void Entry();
public:
	BOOL    InitWebrtc(int nInRateIndex, int nInChannelNum, int nOutSampleRate, int nOutChannelNum, int nOutBitrateAAC);
	BOOL    PushHornPCM(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf);
	BOOL    PushMicFrame(FMS_FRAME & inFrame);
	BOOL    onUdpRecvThreadStop();
private:
	void    doPushToMic(int64_t inPTS, uint8_t * lpData, int nSize);
private:
	void    doEchoMic();
	void    doEchoCancel();
	void    doEchoEncode();
	void    doSaveAudioPCM(void * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID);
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

	int                 m_nWebrtcMS;        // ÿ�δ��������
	int                 m_nWebrtcNN;        // ÿ�δ���������
	short           *   m_lpMicBufNN;       // ��˷�ԭʼ���� => short
	short           *   m_lpHornBufNN;      // ������ԭʼ���� => short
	short           *   m_lpEchoBufNN;      // �������������� => short
	float           *   m_pDMicBufNN;       // ��˷�ԭʼ���� => float
	float           *   m_pDHornBufNN;      // ������ԭʼ���� => float
	float           *   m_pDEchoBufNN;      // �������������� => float
	string              m_strRemainMic;     // ��˷����ʣ������...
	string              m_strRemainEcho;    // ÿ��AACѹ��ʣ������...

	circlebuf           m_circle_mic;		// PCM���ζ��� => ֻ�����˷��������Ƶ����
	circlebuf           m_circle_horn;      // PCM���ζ��� => ֻ�����������������Ƶ����
	circlebuf           m_circle_echo;      // PCM���ζ��� => ֻ��Ż�������֮�����Ƶ����

	CViewCamera     *   m_lpViewCamera;     // ͨ������...
	AVCodec         *   m_lpDecCodec;		// ������...
	AVFrame         *   m_lpDecFrame;		// ����ṹ��...
	AVCodecContext  *   m_lpDecContext;		// ����������...
	AVCodec         *   m_lpEncCodec;		// ѹ����...
	AVFrame         *   m_lpEncFrame;		// ѹ���ṹ��...
	AVCodecContext  *   m_lpEncContext;		// ѹ��������...
	NsxHandle       *   m_lpNsxInst;        // ����ģ����...
	void            *   m_hAEC;             // �����������...
	os_sem_t        *   m_lpAECSem;         // ���������ź���
	pthread_mutex_t     m_AECMutex;         // ��������������
};