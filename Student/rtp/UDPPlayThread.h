
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

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

typedef	map<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => ����ǰ������֡ => ���� => 1/1000
typedef map<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => ��������Ƶ֡ => ���� => 1/1000

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
	int64_t				m_play_next_ns;		// ��һ��Ҫ����֡��ϵͳ����ֵ...
	bool				m_bNeedSleep;		// ��Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
};

class CVideoThread : public CDecoder, public OSThread
{
public:
	CVideoThread(CPlaySDL * lpPlaySDL);
	virtual ~CVideoThread();
	virtual void Entry();
public:
	BOOL	InitVideo(CViewRender * lpViewRender, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
	void	doReBuildSDL();
private:
	int				m_nDstFPS;
	int				m_nDstWidth;
	int				m_nDstHeight;
	string			m_strSPS;
	string			m_strPPS;

	CViewRender  *	m_lpViewRender;	    // ���Ŵ���
	SDL_Window   *  m_sdlScreen;		// SDL����
	SDL_Renderer *  m_sdlRenderer;		// SDL��Ⱦ
    SDL_Texture  *  m_sdlTexture;		// SDL����

	OSMutex			m_Mutex;			// �������
	CPlaySDL	 *  m_lpPlaySDL;		// ���ſ���

	uint8_t      *  m_img_buffer_ptr;   // ��֡ͼ������ռ�
	int             m_img_buffer_size;  // ��֡ͼ�������С
};

class CAudioThread : public CDecoder, public OSThread
{
public:
	CAudioThread(CPlaySDL * lpPlaySDL);
	virtual ~CAudioThread();
	virtual void Entry();
public:
	BOOL	InitAudio(int nRateIndex, int nChannelNum);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
	int     GetCircleSize() { return (m_circle.size/(sizeof(int64_t)+m_out_buffer_size)); }
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
private:
	int					m_audio_rate_index;
	int					m_audio_channel_num;
	int				    m_audio_sample_rate;
	int					m_nSampleDuration;

	SwrContext   *		m_au_convert_ctx;	// ��Ƶ��ʽת��
	uint8_t		 *		m_out_buffer_ptr;	// ��֡����ռ�
	int					m_out_buffer_size;	// ��֡�����С

	circlebuf			m_circle;			// PCM���ݻ��ζ���
	SDL_AudioDeviceID	m_nDeviceID;		// ��Ƶ�豸���

	OSMutex				m_Mutex;			// �������
	CPlaySDL	 *		m_lpPlaySDL;		// ���ſ���
};

class CPlaySDL
{
public:
	CPlaySDL(int64_t inSysZeroNS);
	~CPlaySDL();
public:
	void		PushPacket(int zero_delay_ms, string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime);
	BOOL		InitVideo(CViewRender * lpViewRender, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL		InitAudio(int nRateIndex, int nChannelNum);
	int			GetAPacketSize() { return ((m_lpAudioThread != NULL) ? m_lpAudioThread->GetMapPacketSize() : 0); }
	int			GetVPacketSize() { return ((m_lpVideoThread != NULL) ? m_lpVideoThread->GetMapPacketSize() : 0); }
	int			GetAFrameSize() { return ((m_lpAudioThread != NULL) ? m_lpAudioThread->GetCircleSize() : 0); }
	int			GetVFrameSize() { return ((m_lpVideoThread != NULL) ? m_lpVideoThread->GetMapFrameSize() : 0); }
	int64_t		GetZeroDelayMS() { return m_zero_delay_ms; }
	int64_t		GetSysZeroNS() { return m_sys_zero_ns; }
	int64_t		GetStartPtsMS() { return m_start_pts_ms; }
private:
	bool				m_bFindFirstVKey;	// �Ƿ��ҵ���һ����Ƶ�ؼ�֡��־...
	int64_t				m_sys_zero_ns;		// ϵͳ��ʱ��� => ����ʱ��� => ����...
	int64_t				m_start_pts_ms;		// ��һ֡��PTSʱ�����ʱ��� => ����...
	int64_t				m_zero_delay_ms;	// ��ʱ�趨������ => ���Ը�������Զ�����...

	CVideoThread    *   m_lpVideoThread;	// ��Ƶ�߳�...
	CAudioThread    *   m_lpAudioThread;	// ��Ƶ�߳�...
};
