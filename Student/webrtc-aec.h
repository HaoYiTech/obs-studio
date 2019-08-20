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

	int                 m_nWebrtcMS;        // 每次处理毫秒数
	int                 m_nWebrtcNN;        // 每次处理样本数
	short           *   m_lpMicBufNN;       // 麦克风原始数据 => short
	short           *   m_lpHornBufNN;      // 扬声器原始数据 => short
	short           *   m_lpEchoBufNN;      // 回音消除后数据 => short
	float           *   m_pDMicBufNN;       // 麦克风原始数据 => float
	float           *   m_pDHornBufNN;      // 扬声器原始数据 => float
	float           *   m_pDEchoBufNN;      // 回音消除后数据 => float
	string              m_strRemainMic;     // 麦克风解码剩余数据...
	string              m_strRemainEcho;    // 每次AAC压缩剩余数据...

	circlebuf           m_circle_mic;		// PCM环形队列 => 只存放麦克风解码后的音频数据
	circlebuf           m_circle_horn;      // PCM环形队列 => 只存放扬声器解码后的音频数据
	circlebuf           m_circle_echo;      // PCM环形队列 => 只存放回音消除之后的音频数据

	CViewCamera     *   m_lpViewCamera;     // 通道对象...
	AVCodec         *   m_lpDecCodec;		// 解码器...
	AVFrame         *   m_lpDecFrame;		// 解码结构体...
	AVCodecContext  *   m_lpDecContext;		// 解码器描述...
	AVCodec         *   m_lpEncCodec;		// 压缩器...
	AVFrame         *   m_lpEncFrame;		// 压缩结构体...
	AVCodecContext  *   m_lpEncContext;		// 压缩器描述...
	NsxHandle       *   m_lpNsxInst;        // 降噪模块句柄...
	void            *   m_hAEC;             // 回音消除句柄...
	os_sem_t        *   m_lpAECSem;         // 回音消除信号量
	pthread_mutex_t     m_AECMutex;         // 回音消除互斥体
};