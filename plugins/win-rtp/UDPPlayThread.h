
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/imgutils.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
};

typedef	map<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => ����ǰ������֡ => ���� => 1/1000
typedef map<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => ��������Ƶ֡ => ���� => 1/1000

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
	BOOL	InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
private:
	int				m_nDstFPS;
	int				m_nDstWidth;
	int				m_nDstHeight;
	string			m_strSPS;
	string			m_strPPS;

	OSMutex	         m_Mutex;			// �������
	CPlaySDL    *    m_lpPlaySDL;		// ���ſ���
	obs_source_frame m_obs_frame;		// obs����֡

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

	OSMutex				m_Mutex;			// �������
	CPlaySDL	 *		m_lpPlaySDL;		// ���ſ���
};

class CPlaySDL
{
public:
	CPlaySDL(obs_source_t * lpObsSource, int64_t inSysZeroNS);
	~CPlaySDL();
public:
	void		PushFrame(int zero_delay_ms, string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime);
	BOOL		InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL		InitAudio(int nRateIndex, int nChannelNum);
	int64_t		GetZeroDelayMS() { return m_zero_delay_ms; }
	int64_t		GetSysZeroNS() { return m_sys_zero_ns; }
	int64_t		GetStartPtsMS() { return m_start_pts_ms; }
	obs_source_t * GetObsSource() { return m_lpObsSource; }
private:
	bool				m_bFindFirstVKey;	// �Ƿ��ҵ���һ����Ƶ�ؼ�֡��־...
	int64_t				m_sys_zero_ns;		// ϵͳ��ʱ��� => ����ʱ��� => ����...
	int64_t				m_start_pts_ms;		// ��һ֡��PTSʱ�����ʱ��� => ����...
	int64_t				m_zero_delay_ms;	// ��ʱ�趨������ => ���Ը�������Զ�����...

	CVideoThread    *   m_lpVideoThread;	// ��Ƶ�߳�...
	CAudioThread    *   m_lpAudioThread;	// ��Ƶ�߳�...
	obs_source_t    *   m_lpObsSource;		// obs��Դ����
};
