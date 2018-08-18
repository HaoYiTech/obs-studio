
#pragma once

#include "OSThread.h"

class CViewCamera;
class CDataThread
{
public:
	CDataThread(CViewCamera * lpViewCamera);
	virtual ~CDataThread();
public:
	virtual bool	InitThread() = 0;
	virtual void	ResetEventLoop() = 0;
public:
	void	StartPushThread();
	void	PushFrame(FMS_FRAME & inFrame);
	void	WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void	WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);
public:
	bool			IsFinished() { return m_bFinished; }
	int				GetVideoFPS() { return m_nVideoFPS; }
	int				GetVideoWidth() { return m_nVideoWidth; }
	int				GetVideoHeight() { return m_nVideoHeight; }
	int				GetAudioRateIndex() { return m_audio_rate_index; }
	int				GetAudioChannelNum() { return m_audio_channel_num; }
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	string   &		GetVideoSPS() { return m_strSPS; }
	string   &      GetVideoPPS() { return m_strPPS; }
protected:
	CViewCamera  *  m_lpViewCamera;         // 摄像头窗口对象
	bool			m_bFinished;			// 网络流是否结束了

	int				m_audio_rate_index;     // 音频采样率索引
	int				m_audio_channel_num;    // 音频声道数

	int             m_nVideoFPS;            // 视频帧率
	int				m_nVideoWidth;          // 视频宽度
	int				m_nVideoHeight;         // 视频高度

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC
};

class TaskScheduler;
class ourRTSPClient;
class UsageEnvironment;
class CRtspThread : public OSThread, public CDataThread
{
public:
	CRtspThread(CViewCamera * lpViewCamera);
	virtual ~CRtspThread();
public:
	virtual bool    InitThread();
	virtual void	ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }
private:
	virtual void	Entry();
private:
	string			m_strRtspUrl;           // rtsp链接地址
	TaskScheduler * m_scheduler_;			// rtsp需要的任务对象
	UsageEnvironment * m_env_;				// rtsp需要的环境
	ourRTSPClient * m_rtspClient_;			// rtsp对象
	char m_rtspEventLoopWatchVariable;		// rtsp退出标志 => 事件循环标志，控制它就可以控制任务线程...
};
