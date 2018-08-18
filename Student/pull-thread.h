
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
	CViewCamera  *  m_lpViewCamera;         // ����ͷ���ڶ���
	bool			m_bFinished;			// �������Ƿ������

	int				m_audio_rate_index;     // ��Ƶ����������
	int				m_audio_channel_num;    // ��Ƶ������

	int             m_nVideoFPS;            // ��Ƶ֡��
	int				m_nVideoWidth;          // ��Ƶ���
	int				m_nVideoHeight;         // ��Ƶ�߶�

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
	string			m_strRtspUrl;           // rtsp���ӵ�ַ
	TaskScheduler * m_scheduler_;			// rtsp��Ҫ���������
	UsageEnvironment * m_env_;				// rtsp��Ҫ�Ļ���
	ourRTSPClient * m_rtspClient_;			// rtsp����
	char m_rtspEventLoopWatchVariable;		// rtsp�˳���־ => �¼�ѭ����־���������Ϳ��Կ��������߳�...
};
