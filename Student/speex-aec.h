#pragma once

#include "OSThread.h"
#include "HYDefine.h"
#include <util/threading.h>

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
                      int nHornDelayMS, int nSpeexFrameMS, int nSpeexFilterMS);
	BOOL    PushHornPCM(void * lpBufData, int nBufSize);
	BOOL    PushMicFrame(FMS_FRAME & inFrame);
private:
	void    doEncodeAAC();
	void    doEchoCancel();
	void	doConvertAudio(int64_t in_pts_ms, AVFrame * lpDFrame);
	void    doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel, int nDBCameraID);
private:
	int                 m_in_rate_index;    // �����������
	int                 m_in_channel_num;   // ����������
	int                 m_in_sample_rate;   // ���������
	int                 m_out_channel_num;  // ���������
	int                 m_out_sample_rate;  // ���������
	CViewCamera     *   m_lpViewCamera;     // ͨ������...
	AVCodec         *   m_lpACodec;			// ������...
	AVFrame         *   m_lpDFrame;			// ����ṹ��...
	AVCodecContext  *   m_lpADecoder;		// ����������...
	SwrContext      *   m_out_convert_ctx;	// ��Ƶ��ʽת��
	uint8_t         *   m_max_buffer_ptr;	// ��֡�������ռ�
	int                 m_max_buffer_size;	// ��֡��������С
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
	int                 m_horn_delay_ms;    // �������趨��Ĭ�ϲ����ӳ�ʱ��(����)��SDL�ڲ�����ĺ�����
	os_sem_t        *   m_lpSpeexSem;       // ���������ź���
	pthread_mutex_t     m_SpeexMutex;       // ��������������
	SpeexEchoState_ *   m_lpSpeexEcho;      // ������������
	SpeexPreprocessState_ * m_lpSpeexDen;   // ������������
};