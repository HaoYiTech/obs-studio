
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

typedef multimap<int64_t, AVPacket>		GM_AVPacket;	// DTS => AVPacket  => 解码前的数据帧 => 毫秒 => 1/1000
typedef multimap<int64_t, AVFrame*>		GM_AVFrame;	    // PTS => AVFrame   => 解码后的视频帧 => 毫秒 => 1/1000

class CViewRender;
class CVideoPlay : public OSThread
{
public:
	CVideoPlay(CViewRender * lpViewPlayer, int64_t inSysZeroNS);
	virtual ~CVideoPlay();
private:
	virtual void	Entry();
public:
	//void  onUdpRecvThreadStop() { m_bUdpRecvThreadStop = true; }
	bool    doInitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void    doPushFrame(FMS_FRAME & inFrame, int inCalcPTS);
private:
	void    doSleepTo();
	void    doDecoderFree();
	void    doDecodeFrame();
	void    doDisplayVideo();
	void    doPushPacket(AVPacket & inPacket);
	void	doReBuildSDL(int nPicWidth, int nPicHeight);
	bool    IsCanRebuildSDL(int nPicWidth, int nPicHeight);
private:
	string              m_strSPS;              // 保存SPS
	string              m_strPPS;              // 保存PPS

	AVCodec         *   m_lpDecCodec;		   // 解码器...
	AVFrame         *   m_lpDecFrame;		   // 解码结构体...
	AVCodecContext  *   m_lpDecContext;		   // 解码器描述...
	GM_AVPacket	        m_MapPacket;		   // 解码前的数据帧...
	GM_AVFrame			m_MapFrame;			   // 解码后的数据帧....
	SDL_Window      *   m_sdlScreen;		   // SDL窗口
	SDL_Renderer    *   m_sdlRenderer;		   // SDL渲染
	SDL_Texture     *   m_sdlTexture;		   // SDL纹理
	CViewRender     *   m_lpViewPlayer;        // 播放窗口对象...
	pthread_mutex_t     m_VideoMutex;          // 解码器互斥体对象...
	uint8_t         *   m_img_buffer_ptr;      // 单帧图像输出空间
	int                 m_img_buffer_size;     // 单帧图像输出大小
	int                 m_nSDLTextureWidth;    // SDL纹理的宽度...
	int                 m_nSDLTextureHeight;   // SDL纹理的高度...

	int64_t             m_sys_zero_ns;         // 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_play_next_ns;		   // 下一个要播放帧的系统纳秒值...
	bool				m_bNeedSleep;		   // 休息标志 => 只要有解码或播放就不能休息...

	//bool              m_bUdpRecvThreadStop;  // 右侧线程停止状态标志...
};

class CViewCamera;
class CAudioPlay : public OSThread
{
public:
	CAudioPlay(CViewCamera * lpViewCamera, int64_t inSysZeroNS);
	virtual ~CAudioPlay();
private:
	virtual void	Entry();
public:
	bool    doVolumeEvent(bool bIsVolPlus);
	bool    doInitAudio(int nInRateIndex, int nInChannelNum);
	void    doPushFrame(FMS_FRAME & inFrame, int inCalcPTS);
	void    SetMute(bool bIsMute) { m_bIsMute = bIsMute; }
	float   GetVolRate() { return m_fVolRate; }
private:
	void    doSleepTo();
	void    doDecoderFree();
	void    doMonitorFree();
	void    doDecodeFrame();
	void    doDisplayAudio();
	void    doPushPacket(AVPacket & inPacket);
private:
	AVCodec         *   m_lpDecCodec;		   // 解码器...
	AVFrame         *   m_lpDecFrame;		   // 解码结构体...
	AVCodecContext  *   m_lpDecContext;		   // 解码器描述...
	GM_AVPacket	        m_MapPacket;		   // 解码前的数据帧...
	CViewCamera     *   m_lpViewCamera;        // 通道窗口对象...
	pthread_mutex_t     m_AudioMutex;          // 解码器互斥体对象...
											   
	IMMDevice          *m_device;              // WASAPI设备接口
	IAudioClient       *m_client;              // WASAPI客户端
	IAudioRenderClient *m_render;              // WASAPI渲染器
											   
	int                 m_in_rate_index;       // 输入采样索引
	int                 m_in_channel_num;      // 输入声道数
	int                 m_in_sample_rate;      // 输入采样率
	int                 m_out_channel_num;     // 输出声道数
	int                 m_out_sample_rate;     // 输出采样率
	int                 m_out_frame_bytes;     // 输出每帧占用字节数
	int					m_out_frame_duration;  // 输出每帧持续时间(ms)
	AVSampleFormat      m_in_sample_fmt;       // 输入音频格式
	AVSampleFormat      m_out_sample_fmt;      // 输出音频格式 => SDL需要的是AV_SAMPLE_FMT_S16，WASAPI需要的是AV_SAMPLE_FMT_FLT...

	resample_info       m_horn_sample_info;    // 扬声器需要的音频样本格式
	audio_resampler_t * m_horn_resampler;      // 解码后原始样本数据转换成扬声器样本格式 => dec 转 horn

	int64_t             m_sys_zero_ns;         // 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_play_next_ns;		   // 下一个要播放帧的系统纳秒值...
	bool				m_bNeedSleep;		   // 休息标志 => 只要有解码或播放就不能休息...
	int                 m_frame_num;           // PCM数据帧总数
	circlebuf			m_circle;			   // PCM数据环形队列
	string              m_strHorn;             // PCM单帧数据块
	bool                m_bIsMute;             // 是否静音标志
	float               m_fVolRate;            // 音量放大倍率
};
