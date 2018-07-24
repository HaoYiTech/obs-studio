
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "myRTSPClient.h"

class CPushThread;
class CRtspThread : public OSThread
{
public:
	CRtspThread();
	~CRtspThread();
public:
	void			StartPushThread();
	void			PushFrame(FMS_FRAME & inFrame);
	BOOL			InitRtsp(CPushThread * lpPushThread, BOOL bUsingTCP, string & strRtspUrl);
	int				GetVideoFPS() { return m_nVideoFPS; }
	int				GetVideoWidth(){ return m_nVideoWidth; }
	int				GetVideoHeight() { return m_nVideoHeight; }
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }
	void			WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void			WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }

	string   &		GetVideoSPS() { return m_strSPS; }
	string   &      GetVideoPPS() { return m_strPPS; }
	int				GetAudioRateIndex() { return m_audio_rate_index; }
	int				GetAudioChannelNum() { return m_audio_channel_num; }
private:
	virtual void	Entry();
private:
	OSMutex			m_Mutex;
	string			m_strRtspUrl;
	CPushThread  *  m_lpPushThread;

	bool			m_bFinished;			// �������Ƿ������

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int             m_nVideoFPS;
	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	TaskScheduler * m_scheduler_;			// rtsp��Ҫ���������
	UsageEnvironment * m_env_;				// rtsp��Ҫ�Ļ���
	ourRTSPClient * m_rtspClient_;			// rtsp����
	char m_rtspEventLoopWatchVariable;		// rtsp�˳���־ => �¼�ѭ����־���������Ϳ��Կ��������߳�...
};
