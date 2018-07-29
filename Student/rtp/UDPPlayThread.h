
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

typedef	map<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => 解码前的数据帧 => 毫秒 => 1/1000
typedef map<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => 解码后的视频帧 => 毫秒 => 1/1000

// 第三版：音视频线程分开播放...
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
	AVCodec         *   m_lpCodec;			// 解码器...
	AVFrame         *   m_lpDFrame;			// 解码结构体...
	AVCodecContext  *   m_lpDecoder;		// 解码器描述...
	GM_MapPacket		m_MapPacket;		// 解码前的数据帧...
	GM_MapFrame			m_MapFrame;			// 解码后的数据帧....
	int64_t				m_play_next_ns;		// 下一个要播放帧的系统纳秒值...
	bool				m_bNeedSleep;		// 休息标志 => 只要有解码或播放就不能休息...
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

	CViewRender  *	m_lpViewRender;	    // 播放窗口
	SDL_Window   *  m_sdlScreen;		// SDL窗口
	SDL_Renderer *  m_sdlRenderer;		// SDL渲染
    SDL_Texture  *  m_sdlTexture;		// SDL纹理

	OSMutex			m_Mutex;			// 互斥对象
	CPlaySDL	 *  m_lpPlaySDL;		// 播放控制

	uint8_t      *  m_img_buffer_ptr;   // 单帧图像输出空间
	int             m_img_buffer_size;  // 单帧图像输出大小
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

	SwrContext   *		m_au_convert_ctx;	// 音频格式转换
	uint8_t		 *		m_out_buffer_ptr;	// 单帧输出空间
	int					m_out_buffer_size;	// 单帧输出大小

	circlebuf			m_circle;			// PCM数据环形队列
	SDL_AudioDeviceID	m_nDeviceID;		// 音频设备编号

	OSMutex				m_Mutex;			// 互斥对象
	CPlaySDL	 *		m_lpPlaySDL;		// 播放控制
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
	bool				m_bFindFirstVKey;	// 是否找到第一个视频关键帧标志...
	int64_t				m_sys_zero_ns;		// 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_start_pts_ms;		// 第一帧的PTS时间戳计时起点 => 毫秒...
	int64_t				m_zero_delay_ms;	// 延时设定毫秒数 => 可以根据情况自动调节...

	CVideoThread    *   m_lpVideoThread;	// 视频线程...
	CAudioThread    *   m_lpAudioThread;	// 音频线程...
};
