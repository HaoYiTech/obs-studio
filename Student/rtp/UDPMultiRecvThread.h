
#pragma once

#include "HYDefine.h"
#include "OSThread.h"

class CPlaySDL;
class UDPSocket;
class CViewRender;
class CUDPMultiRecvThread : public OSThread
{
public:
	CUDPMultiRecvThread(CViewRender * lpViewRender);
	virtual ~CUDPMultiRecvThread();
	virtual void Entry();
public:
	BOOL            InitThread();
	bool            doVolumeEvent(bool bIsVolPlus);
	void            doResetMulticastIPSend();
private:
	void            ClosePlayer();
	void            ClearAllSocket();
	BOOL            BuildDataSocket();
	BOOL            BuildLoseSocket();

	void			doSendSupplyCmd(bool bIsAudio);
	void            doCheckRecvTimeout();
	void			doRecvPacket();
	void			doSleepTo();

	void			doProcServerHeader(char * lpBuffer, int inRecvLen);
	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);

	void			doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID);
	void			doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID);
	void			doParseFrame(bool bIsAudio);

	void			doServerMinSeq(bool bIsAudio, uint32_t inMinSeq);
private:
	enum {
		kCmdSendCreate = 0,					// 开始发送 => 创建命令状态
		kCmdConnetOK = 1,					// 接入交互完毕，只是做为一个阻止继续发送准备就绪命令的标志
	} m_nCmdState;							// 命令状态变量...

	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	int64_t			m_time_zero_ns;			// 超时0点时刻 => 纳秒...
	int64_t			m_sys_zero_ns;			// 系统计时零点 => 第一个数据包到达的系统时刻点 => 纳秒...
	int				m_server_rtt_ms;		// Server => 网络往返延迟值 => 毫秒
	int				m_server_rtt_var_ms;	// Server => 网络抖动时间差 => 毫秒
	int				m_server_cache_time_ms;	// Server => 缓冲评估时间   => 毫秒 => 就是播放延时时间
	int				m_nMaxResendCount;		// 当前丢包最大重发次数

	rtp_supply_t	m_rtp_supply;			// RTP补包命令结构体
	rtp_header_t	m_rtp_header;			// RTP序列头结构体 => 接收 => 来自推流端...
	string          m_strSeqHeader;         // 推流端上传的序列头命令包...

	circlebuf		m_audio_circle;			// 音频环形队列
	circlebuf		m_video_circle;			// 视频环形队列

	bool			m_bFirstAudioSeq;		// 音频第一个数据包已收到标志...
	bool			m_bFirstVideoSeq;		// 视频第一个数据包已收到标志...
	uint32_t		m_nAudioMaxPlaySeq;		// 音频RTP当前最大播放序列号 => 最大连续有效序列号...
	uint32_t		m_nVideoMaxPlaySeq;		// 视频RTP当前最大播放序列号 => 最大连续有效序列号...

	GM_MapLose		m_AudioMapLose;			// 音频检测到的丢包集合队列...
	GM_MapLose		m_VideoMapLose;			// 视频检测到的丢包集合队列...

	UDPSocket   *   m_lpUdpData;			// 组播数据接收套接字对象...
	UDPSocket   *   m_lpUdpLose;			// 组播丢包发送套接字对象...
	CPlaySDL    *   m_lpPlaySDL;            // SDL播放管理器...
	CViewRender *   m_lpViewRender;         // 渲染窗口对象...
};
