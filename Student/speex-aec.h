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
	int                 m_in_rate_index;    // 输入采样索引
	int                 m_in_channel_num;   // 输入声道数
	int                 m_in_sample_rate;   // 输入采样率
	int                 m_out_channel_num;  // 输出声道数
	int                 m_out_sample_rate;  // 输出采样率

	int                 m_aac_out_bitrate;  // AAC压缩输出码流
	int                 m_aac_frame_count;  // AAC已压缩帧计数器
	int                 m_aac_nb_samples;   // AAC包含每帧样本数
	int                 m_aac_frame_size;   // AAC包含每帧PCM长度
	uint8_t         *   m_aac_frame_ptr;    // AAC原始PCM空间数据

	resample_info       m_aac_sample_info;  // AAC输出需要的音频样本格式
	resample_info       m_echo_sample_info; // 回音消除需要的音频样本格式
	audio_resampler_t * m_mic_resampler;    // 麦克风原始样本数据转换成回音消除样本格式 => mic 转 echo
	audio_resampler_t * m_horn_resampler;   // 扬声器原始样本数据转换成回音消除样本格式 => horn 转 echo
	audio_resampler_t * m_echo_resampler;   // 回音原始样本数据转换成AAC压缩原始样本格式 => echo 转 aac

	CViewCamera     *   m_lpViewCamera;     // 通道对象...
	AVCodec         *   m_lpDeCodec;		// 解码器...
	AVFrame         *   m_lpDeFrame;		// 解码结构体...
	AVCodecContext  *   m_lpDeContext;		// 解码器描述...
	AVCodec         *   m_lpEnCodec;		// 压缩器...
	AVFrame         *   m_lpEnFrame;		// 压缩结构体...
	AVCodecContext  *   m_lpEnContext;		// 压缩器描述...
	int                 m_nSpeexNN;         // 每次处理样本数
	int                 m_nSpeexTAIL;       // 尾音样本长度
	short           *   m_lpMicBufNN;       // 麦克风原始数据
	short           *   m_lpHornBufNN;      // 扬声器原始数据
	short           *   m_lpEchoBufNN;      // 回音消除后数据
	circlebuf           m_circle_mic;		// PCM环形队列 => 只存放麦克风解码后的音频数据
	circlebuf           m_circle_horn;      // PCM环形队列 => 只存放扬声器解码后的音频数据
	circlebuf           m_circle_echo;      // PCM环形队列 => 只存放回音消除之后的音频数据
	int64_t             m_mic_pts_ms_zero;  // 麦克风输入第一个AAC数据帧的PTS(毫秒) => 0点时间，便于压缩后的PTS计算，默认-1
	int64_t             m_mic_sys_ns_zero;  // 麦克风解压第一个AAC数据帧后的系统时间(纳秒) => 0点时间，便于与扬声器比对，寻找回音消除时刻点，默认-1
	int64_t             m_horn_sys_ns_zero; // 扬声器输入第一个PCM数据包后的系统时间(纳秒) => 0点时间，便于与麦克风比对，寻找回音消除时刻点，默认-1
	int                 m_horn_delay_ms;    // 扬声器设定的默认播放延迟时间(毫秒)，SDL内部缓存的毫秒数 => 使用WSAPI之后，设定为0毫秒...
	os_sem_t        *   m_lpSpeexSem;       // 回音消除信号量
	pthread_mutex_t     m_SpeexMutex;       // 回音消除互斥体
	SpeexEchoState_ *   m_lpSpeexEcho;      // 回音消除对象
	SpeexPreprocessState_ * m_lpSpeexDen;   // 回音消除对象
};