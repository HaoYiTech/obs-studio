
#pragma once

#include <util/threading.h>
#include "HYDefine.h"
#include "OSThread.h"

class CPlaySDL;
class UDPSocket;
class CViewRender;
class CUDPMultiSendThread;
class CUDPRecvThread : public OSThread
{
public:
	CUDPRecvThread(CViewRender * lpViewRender, int nDBRoomID, int nDBCameraID);
	virtual ~CUDPRecvThread();
	virtual void Entry();
public:
	BOOL            doIsServerLose(uint8_t inPType, uint32_t inLoseSeq);
	BOOL			InitThread(string & strUdpAddr, int nUdpPort);
	bool            doVolumeEvent(bool bIsVolPlus);
	void            doResetMulticastIPSend();
	void            doDeleteExAudioThread();
private:
	void            ResetExAudio();
	void            CloseMultiSend();
	void			ClosePlayer();
	void			CloseSocket();
	void			doSendCreateCmd();
	void			doSendDeleteCmd();
	void			doSendSupplyCmd(uint8_t inPType);
	void			doSendDetectCmd();
	void			doRecvPacket();
	void			doSleepTo();

	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);

	void			doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID);
	void			doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID);
	void			doParseFrame(uint8_t inPType);

	uint32_t		doCalcMaxConSeq(uint8_t inPType);
	void			doServerMinSeq(uint8_t inPType, uint32_t inMinSeq);
private:
	enum {
		kCmdSendCreate	= 0,				// 开始发送 => 创建命令状态
		kCmdConnectOK	= 1,				// 接入交互完毕，已经开始接收音视频数据了
	} m_nCmdState;							// 命令状态变量...

	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	UDPSocket	*	m_lpUDPSocket;			// UDP对象

	uint16_t		m_HostServerPort;		// 服务器端口 => host
	uint32_t	    m_HostServerAddr;		// 服务器地址 => host
	string          m_HostServerStr;        // 服务器地址 => string

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	int				m_dt_to_dir;			// 发包路线方向 => TO_SERVER | TO_P2P
	int				m_server_rtt_ms;		// Server => 网络往返延迟值 => 毫秒
	int				m_server_rtt_var_ms;	// Server => 网络抖动时间差 => 毫秒
	int				m_server_cache_time_ms;	// Server => 缓冲评估时间   => 毫秒 => 就是播放延时时间
	int				m_nMaxResendCount;		// 当前丢包最大重发次数
	
	rtp_detect_t	m_rtp_detect;			// RTP探测命令结构体
	rtp_create_t	m_rtp_create;			// RTP创建房间和直播结构体
	rtp_delete_t	m_rtp_delete;			// RTP删除房间和直播结构体
	rtp_supply_t	m_rtp_supply;			// RTP补包命令结构体

	rtp_header_t	m_rtp_header;			// RTP序列头结构体 => 接收 => 来自老师推流端...
	string          m_strSeqHeader;         // 老师推流端上传的序列头命令包...

	int64_t			m_sys_zero_ns;			// 系统计时零点 => 第一个数据包到达的系统时刻点 => 纳秒...
	int64_t			m_next_create_ns;		// 下次发送创建命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_detect_ns;		// 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...

	circlebuf		m_audio_circle;			// 音频环形队列
	circlebuf		m_video_circle;			// 视频环形队列

	bool			m_bFirstAudioSeq;		// 音频第一个数据包已收到标志...
	bool			m_bFirstVideoSeq;		// 视频第一个数据包已收到标志...
	uint32_t		m_nAudioMaxPlaySeq;		// 音频RTP当前最大播放序列号 => 最大连续有效序列号...
	uint32_t		m_nVideoMaxPlaySeq;		// 视频RTP当前最大播放序列号 => 最大连续有效序列号...

	bool            m_Ex_bFirstAudioSeq;    // 扩展音频第一个数据包已收到标志...
	uint16_t        m_Ex_wAudioChangeNum;   // 扩展音频变化次数...
	uint32_t        m_Ex_nAudioMaxPlaySeq;  // 扩展音频RTP当前最大播放序列号 => 最大连续有效序列号...
	GM_MapLose		m_Ex_AudioMapLose;		// 扩展音频检测到的丢包集合队列...
	circlebuf		m_Ex_audio_circle;		// 扩展音频环形队列

	GM_MapLose		m_AudioMapLose;			// 音频检测到的丢包集合队列...
	GM_MapLose		m_VideoMapLose;			// 视频检测到的丢包集合队列...
	pthread_mutex_t m_MutexLose;            // 丢包互斥保护对象...

	CPlaySDL            * m_lpPlaySDL;           // SDL播放管理器...
	CViewRender         * m_lpViewRender;        // 渲染窗口对象...
	CUDPMultiSendThread * m_lpMultiSendThread;   // 组播发送者线程对象...
};