
#pragma once

#include "OSThread.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <util/threading.h>
#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include "SDL2/SDL.h"
};

typedef map<int64_t, AVPacket>		GM_AVPacket;	// DTS => AVPacket  => ����ǰ������֡ => ���� => 1/1000
typedef map<int64_t, AVFrame*>		GM_AVFrame;	    // PTS => AVFrame   => ��������Ƶ֡ => ���� => 1/1000

/*class CVideoPlay : public OSThread
{
public:
	CVideoPlay(CViewCamera * lpViewCamera);
	virtual ~CVideoPlay();
private:
	virtual void	Entry();
public:
	bool    doInitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void    doPushFrame(FMS_FRAME & inFrame, int inCalcPTS);
private:
	void    doSleepTo();
	void    doDecoderFree();
	void    doDecodeFrame();
	void    doDisplayVideo();
	void	doReBuildSDL();
	void    doPushPacket(AVPacket & inPacket);
private:
	int				    m_nDstFPS;
	int				    m_nDstWidth;
	int				    m_nDstHeight;
	string              m_strSPS;
	string              m_strPPS;

	AVCodec         *   m_lpDecCodec;		   // ������...
	AVFrame         *   m_lpDecFrame;		   // ����ṹ��...
	AVCodecContext  *   m_lpDecContext;		   // ����������...
	GM_AVPacket	        m_MapPacket;		   // ����ǰ������֡...
	GM_AVFrame			m_MapFrame;			   // ����������֡....
	SDL_Window      *   m_sdlScreen;		   // SDL����
	SDL_Renderer    *   m_sdlRenderer;		   // SDL��Ⱦ
	SDL_Texture     *   m_sdlTexture;		   // SDL����
	CViewCamera     *   m_lpViewCamera;        // ͨ�����ڶ���...
	pthread_mutex_t     m_VideoMutex;          // ���������������...
	uint8_t         *   m_img_buffer_ptr;      // ��֡ͼ������ռ�
	int                 m_img_buffer_size;     // ��֡ͼ�������С

	int64_t				m_play_next_ns;		   // ��һ��Ҫ����֡��ϵͳ����ֵ...
	bool				m_bNeedSleep;		   // ��Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
};*/

class CViewCamera;
class CAudioPlay : public OSThread
{
public:
	CAudioPlay(CViewCamera * lpViewCamera);
	virtual ~CAudioPlay();
private:
	virtual void	Entry();
public:
	bool    doInitAudio(int nInRateIndex, int nInChannelNum);
	void    doPushFrame(FMS_FRAME & inFrame, int inCalcPTS);
private:
	void    doSleepTo();
	void    doDecoderFree();
	void    doMonitorFree();
	void    doDecodeFrame();
	void    doDisplayAudio();
	void    doPushPacket(AVPacket & inPacket);
private:
	AVCodec         *   m_lpDecCodec;		   // ������...
	AVFrame         *   m_lpDecFrame;		   // ����ṹ��...
	AVCodecContext  *   m_lpDecContext;		   // ����������...
	GM_AVPacket	        m_MapPacket;		   // ����ǰ������֡...
	CViewCamera     *   m_lpViewCamera;        // ͨ�����ڶ���...
	pthread_mutex_t     m_AudioMutex;          // ���������������...
											   
	IMMDevice          *m_device;              // WASAPI�豸�ӿ�
	IAudioClient       *m_client;              // WASAPI�ͻ���
	IAudioRenderClient *m_render;              // WASAPI��Ⱦ��
											   
	int                 m_in_rate_index;       // �����������
	int                 m_in_channel_num;      // ����������
	int                 m_in_sample_rate;      // ���������
	int                 m_out_channel_num;     // ���������
	int                 m_out_sample_rate;     // ���������
	int                 m_out_frame_bytes;     // ���ÿ֡ռ���ֽ���
	int					m_out_frame_duration;  // ���ÿ֡����ʱ��(ms)
	AVSampleFormat      m_in_sample_fmt;       // ������Ƶ��ʽ
	AVSampleFormat      m_out_sample_fmt;      // �����Ƶ��ʽ => SDL��Ҫ����AV_SAMPLE_FMT_S16��WASAPI��Ҫ����AV_SAMPLE_FMT_FLT...

	resample_info       m_horn_sample_info;    // ��������Ҫ����Ƶ������ʽ
	audio_resampler_t * m_horn_resampler;      // �����ԭʼ��������ת����������������ʽ => dec ת horn

	int64_t				m_play_next_ns;		   // ��һ��Ҫ����֡��ϵͳ����ֵ...
	bool				m_bNeedSleep;		   // ��Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
	int                 m_frame_num;           // PCM����֡����
	circlebuf			m_circle;			   // PCM���ݻ��ζ���
	string              m_strHorn;             // PCM��֡���ݿ�
};
