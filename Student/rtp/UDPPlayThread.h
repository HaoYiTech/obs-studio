
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

typedef	multimap<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => 解码前的数据帧 => 毫秒 => 1/1000
typedef multimap<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => 解码后的视频帧 => 毫秒 => 1/1000

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
	bool				m_bNeedSleep;		// 休息标志 => 只要有解码或播放就不能休息...
	int64_t				m_play_next_ns;		// 下一个要播放帧的系统纳秒值...
	pthread_mutex_t     m_DecoderMutex;     // 解码器互斥体对象...
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
	string			m_strSPS;              // 保存SPS
	string			m_strPPS;              // 保存PPS
	CPlaySDL	 *  m_lpPlaySDL;           // 播放控制
	CViewRender  *	m_lpViewRender;	       // 播放窗口
	SDL_Window   *  m_sdlScreen;           // SDL窗口
	SDL_Renderer *  m_sdlRenderer;         // SDL渲染
    SDL_Texture  *  m_sdlTexture;          // SDL纹理
	uint8_t      *  m_img_buffer_ptr;      // 单帧图像输出空间
	int             m_img_buffer_size;     // 单帧图像输出大小
	int             m_nSDLTextureWidth;    // SDL纹理的宽度...
	int             m_nSDLTextureHeight;   // SDL纹理的高度...
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
	bool                m_bIsExAudio;          // 扩展音频标志
	int					m_in_rate_index;       // 输入采样索引
	int					m_in_channel_num;      // 输入声道数
	int				    m_in_sample_rate;      // 输入采样率
	int                 m_out_channel_num;     // 输出声道数
	int                 m_out_sample_rate;     // 输出采样率
	int                 m_out_frame_bytes;     // 输出每帧占用字节数
	int					m_out_frame_duration;  // 输出每帧持续时间(ms)
	AVSampleFormat		m_out_sample_fmt;      // 输出采样格式 => SDL需要的是AV_SAMPLE_FMT_S16，WASAPI需要的是AV_SAMPLE_FMT_FLT...

	SwrContext   *		m_out_convert_ctx;	   // 音频格式转换
	uint8_t		 *		m_max_buffer_ptr;	   // 单帧最大输出空间
	int					m_max_buffer_size;	   // 单帧最大输出大小

	int                 m_frame_num;           // PCM数据帧总数
	circlebuf			m_circle;			   // PCM数据环形队列

	CPlaySDL	 *		m_lpPlaySDL;		   // 播放控制

	IMMDevice          *m_device;              // WASAPI设备接口
	IAudioClient       *m_client;              // WASAPI客户端
	IAudioRenderClient *m_render;              // WASAPI渲染器
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
	float               m_fVolRate;         // 音量放大倍率
	bool				m_bFindFirstVKey;	// 是否找到第一个视频关键帧标志...
	int64_t				m_sys_zero_ns;		// 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_start_pts_ms;		// 第一帧的PTS时间戳计时起点 => 毫秒...
	int64_t				m_zero_delay_ms;	// 延时设定毫秒数 => 可以根据情况自动调节...
	int64_t             m_ex_start_ms;      // 扩展音频第一帧PTS时间戳计时起点 => 毫秒...
	int64_t             m_ex_zero_ns;       // 扩展音频系统计时零点 => 启动时间戳 => 纳秒...
	uint32_t            m_ex_format;        // 扩展音频的格式信息，包含rateIndex|channelNum...

	CAudioThread    *   m_lpExAudioThread;	// 扩展音频...
	CAudioThread    *   m_lpAudioThread;	// 音频线程...
	CVideoThread    *   m_lpVideoThread;	// 视频线程...
	CViewRender     *   m_lpViewRender;     // 渲染窗口...
};
