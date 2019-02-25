
#pragma once

#include "OSThread.h"
#include <util/threading.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

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

typedef	multimap<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => ����ǰ������֡ => ���� => 1/1000
typedef multimap<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => ��������Ƶ֡ => ���� => 1/1000

// �����棺����Ƶ�̷ֿ߳�����...
class CViewRender;
class CPlaySDL;
class CDecoder
{
public:
	CDecoder();
	~CDecoder();
public:
	void		doSleepTo();
	void		doPushPacket(AVPacket & inPacket);
	int			GetMapPacketSize() { return m_MapPacket.size(); }
	int         GetMapFrameSize() { return m_MapFrame.size(); }
protected:
	AVCodec         *   m_lpCodec;			// ������...
	AVFrame         *   m_lpDFrame;			// ����ṹ��...
	AVCodecContext  *   m_lpDecoder;		// ����������...
	GM_MapPacket		m_MapPacket;		// ����ǰ������֡...
	GM_MapFrame			m_MapFrame;			// ����������֡....
	bool				m_bNeedSleep;		// ��Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
	int64_t				m_play_next_ns;		// ��һ��Ҫ����֡��ϵͳ����ֵ...
	pthread_mutex_t     m_DecoderMutex;     // ���������������...
};

class CVideoThread : public CDecoder, public OSThread
{
public:
	CVideoThread(CPlaySDL * lpPlaySDL, CViewRender * lpViewRender);
	virtual ~CVideoThread();
	virtual void Entry();
public:
	BOOL	InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
	void	doReBuildSDL(int nPicWidth, int nPicHeight);
	bool    IsCanRebuildSDL(int nPicWidth, int nPicHeight);
private:
	string			m_strSPS;              // ����SPS
	string			m_strPPS;              // ����PPS
	CPlaySDL	 *  m_lpPlaySDL;           // ���ſ���
	CViewRender  *	m_lpViewRender;	       // ���Ŵ���
	SDL_Window   *  m_sdlScreen;           // SDL����
	SDL_Renderer *  m_sdlRenderer;         // SDL��Ⱦ
    SDL_Texture  *  m_sdlTexture;          // SDL����
	uint8_t      *  m_img_buffer_ptr;      // ��֡ͼ������ռ�
	int             m_img_buffer_size;     // ��֡ͼ�������С
	int             m_nSDLTextureWidth;    // SDL����Ŀ��...
	int             m_nSDLTextureHeight;   // SDL����ĸ߶�...
};

class CAudioThread : public CDecoder, public OSThread
{
public:
	CAudioThread(CPlaySDL * lpPlaySDL, bool bIsExAudio = false);
	virtual ~CAudioThread();
	virtual void Entry();
public:
	BOOL	InitAudio(int nInRateIndex, int nInChannelNum);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
	int     GetCircleSize() { return m_frame_num; }
private:
	void	doConvertAudio(int64_t in_pts_ms, AVFrame * lpDFrame);
	void	doDecodeFrame();
	void	doDisplaySDL();
	void    doMonitorFree();
private:
	bool                m_bIsExAudio;          // ��չ��Ƶ��־
	int					m_in_rate_index;       // �����������
	int					m_in_channel_num;      // ����������
	int				    m_in_sample_rate;      // ���������
	int                 m_out_channel_num;     // ���������
	int                 m_out_sample_rate;     // ���������
	int                 m_out_frame_bytes;     // ���ÿ֡ռ���ֽ���
	int					m_out_frame_duration;  // ���ÿ֡����ʱ��(ms)
	AVSampleFormat		m_out_sample_fmt;      // ���������ʽ => SDL��Ҫ����AV_SAMPLE_FMT_S16��WASAPI��Ҫ����AV_SAMPLE_FMT_FLT...

	SwrContext   *		m_out_convert_ctx;	   // ��Ƶ��ʽת��
	uint8_t		 *		m_max_buffer_ptr;	   // ��֡�������ռ�
	int					m_max_buffer_size;	   // ��֡��������С

	int                 m_frame_num;           // PCM����֡����
	circlebuf			m_circle;			   // PCM���ݻ��ζ���

	CPlaySDL	 *		m_lpPlaySDL;		   // ���ſ���

	IMMDevice          *m_device;              // WASAPI�豸�ӿ�
	IAudioClient       *m_client;              // WASAPI�ͻ���
	IAudioRenderClient *m_render;              // WASAPI��Ⱦ��
};

class CPlaySDL
{
public:
	CPlaySDL(CViewRender * lpViewRender, int64_t inSysZeroNS);
	~CPlaySDL();
public:
	void        doDeleteExAudioThread();
	void        PushExAudio(string & inData, uint32_t inSendTime, uint32_t inExFormat);
	void		PushPacket(int zero_delay_ms, string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime);
	BOOL		InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL		InitAudio(int nInRateIndex, int nInChannelNum);
	bool        doVolumeEvent(bool bIsVolPlus);
	float       GetVolRate() { return m_fVolRate; }
	int			GetAPacketSize() { return ((m_lpAudioThread != NULL) ? m_lpAudioThread->GetMapPacketSize() : 0); }
	int			GetVPacketSize() { return ((m_lpVideoThread != NULL) ? m_lpVideoThread->GetMapPacketSize() : 0); }
	int			GetAFrameSize() { return ((m_lpAudioThread != NULL) ? m_lpAudioThread->GetCircleSize() : 0); }
	int			GetVFrameSize() { return ((m_lpVideoThread != NULL) ? m_lpVideoThread->GetMapFrameSize() : 0); }
	int64_t		GetZeroDelayMS() { return m_zero_delay_ms; }
	int64_t		GetSysZeroNS() { return m_sys_zero_ns; }
	int64_t		GetStartPtsMS() { return m_start_pts_ms; }
	int64_t     GetExStartPtsMS() { return m_ex_start_ms; }
	int64_t     GetExSysZeroNS() { return m_ex_zero_ns; }
	void        ResetExAudioFormat() { m_ex_format = 0; }
private:
	float               m_fVolRate;         // �����Ŵ���
	bool				m_bFindFirstVKey;	// �Ƿ��ҵ���һ����Ƶ�ؼ�֡��־...
	int64_t				m_sys_zero_ns;		// ϵͳ��ʱ��� => ����ʱ��� => ����...
	int64_t				m_start_pts_ms;		// ��һ֡��PTSʱ�����ʱ��� => ����...
	int64_t				m_zero_delay_ms;	// ��ʱ�趨������ => ���Ը�������Զ�����...
	int64_t             m_ex_start_ms;      // ��չ��Ƶ��һ֡PTSʱ�����ʱ��� => ����...
	int64_t             m_ex_zero_ns;       // ��չ��Ƶϵͳ��ʱ��� => ����ʱ��� => ����...
	uint32_t            m_ex_format;        // ��չ��Ƶ�ĸ�ʽ��Ϣ������rateIndex|channelNum...

	CAudioThread    *   m_lpExAudioThread;	// ��չ��Ƶ...
	CAudioThread    *   m_lpAudioThread;	// ��Ƶ�߳�...
	CVideoThread    *   m_lpVideoThread;	// ��Ƶ�߳�...
	CViewRender     *   m_lpViewRender;     // ��Ⱦ����...
};
