
#pragma once

#include "HYDefine.h"
#include "OSMutex.h"
#include "OSThread.h"

class UDPSocket;
class CDataThread;
class CUDPSendThread : public OSThread
{
public:
	CUDPSendThread(CDataThread * lpDataThread, int nDBRoomID, int nDBCameraID);
	virtual ~CUDPSendThread();
	virtual void Entry();
public:
	string    &    GetServerAddrStr() { return m_HostServerStr; }
	int            GetServerPortInt() { return m_HostServerPort; }
	int            GetSendTotalKbps() { return (m_bNeedDelete ? -1 : m_total_output_kbps); }
	int            GetSendAudioKbps() { return (m_bNeedDelete ? -1 : m_audio_output_kbps); }
	int            GetSendVideoKbps() { return (m_bNeedDelete ? -1 : m_video_output_kbps); }
	bool           IsNeedDelete() { return m_bNeedDelete; }
public:
	BOOL			InitThread(string & strUdpAddr, int nUdpPort, int nAudioRateIndex, int nAudioChannelNum);
	BOOL			PushFrame(FMS_FRAME & inFrame);
protected:
	BOOL			InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL			InitAudio(int nRateIndex, int nChannelNum);
	BOOL			ParseAVHeader(int nAudioRateIndex, int nAudioChannelNum);
private:
	void			CloseSocket();
	void			doCalcAVBitRate();
	void			doSendCreateCmd();
	void			doSendHeaderCmd();
	void			doSendDeleteCmd();
	void			doSendDetectCmd();
	void			doSendLosePacket(bool bIsAudio);
	void			doSendPacket(bool bIsAudio);
	void			doRecvPacket();
	void			doSleepTo();

	void			doProcServerCreate(char * lpBuffer, int inRecvLen);
	void			doProcServerHeader(char * lpBuffer, int inRecvLen);
	void			doProcServerReady(char * lpBuffer, int inRecvLen);

	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagSupplyProcess(char * lpBuffer, int inRecvLen);

	void			doProcMaxConSeq(bool bIsAudio, uint32_t inMaxConSeq);

	void			doCalcAVJamFlag();
private:
	enum {
		kCmdSendCreate	= 0,				// 开始发送 => 创建命令状态
		kCmdSendHeader	= 1,				// 开始发送 => 序列头命令状态
		kCmdWaitReady	= 2,				// 等待接收 => 来自观看端的准备就绪命令状态
		kCmdSendAVPack	= 3,				// 开始发送 => 音视频数据包状态
		kCmdUnkownState = 4,				// 未知状态 => 线程已经退出		
	} m_nCmdState;							// 命令状态变量...

	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	OSMutex			m_Mutex;				// 互斥对象
	UDPSocket	*	m_lpUDPSocket;			// UDP对象
	CDataThread *   m_lpDataThread;         // 推流管理

	uint16_t		m_HostServerPort;		// 服务器端口 => host
	uint32_t	    m_HostServerAddr;		// 服务器地址 => host
	string          m_HostServerStr;        // 服务器地址 => string

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	bool            m_bNeedDelete;			// 删除标志 => 没有用户接入或网络严重拥塞...
	int				m_dt_to_dir;			// 发包路线方向 => TO_SERVER | TO_P2P
	int				m_p2p_rtt_ms;			// P2P    => 网络往返延迟值 => 毫秒
	int				m_p2p_rtt_var_ms;		// P2P    => 网络抖动时间差 => 毫秒
	int				m_server_rtt_ms;		// Server => 网络往返延迟值 => 毫秒
	int				m_server_rtt_var_ms;	// Server => 网络抖动时间差 => 毫秒

	rtp_detect_t	m_rtp_detect;			// RTP探测命令结构体
	rtp_create_t	m_rtp_create;			// RTP创建房间和直播结构体
	rtp_delete_t	m_rtp_delete;			// RTP删除房间和直播结构体
	rtp_header_t	m_rtp_header;			// RTP序列头结构体

	rtp_ready_t		m_rtp_ready;			// RTP准备继续结构体 => 接收 => 保存观看端信息

	int64_t			m_next_create_ns;		// 下次发送创建命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_header_ns;		// 下次发送序列头命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_detect_ns;		// 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...

	circlebuf		m_audio_circle;			// 音频环形队列
	circlebuf		m_video_circle;			// 视频环形队列

	uint32_t		m_nAudioCurPackSeq;		// 音频RTP当前打包序列号
	uint32_t		m_nAudioCurSendSeq;		// 音频RTP当前发送序列号

	uint32_t		m_nVideoCurPackSeq;		// 视频RTP当前打包序列号
	uint32_t		m_nVideoCurSendSeq;		// 视频RTP当前发送序列号

	GM_MapLose		m_AudioMapLose;			// 音频检测到的丢包集合队列...
	GM_MapLose		m_VideoMapLose;			// 视频检测到的丢包集合队列...

	int64_t			m_start_time_ns;		// 码流计算启动时间...
	int64_t			m_total_time_ms;		// 总的持续毫秒数...

	int64_t			m_audio_input_bytes;	// 音频输入总字节数...
	int64_t			m_video_input_bytes;	// 视频输入总字节数...
	int				m_audio_input_kbps;		// 音频输入平均码率...
	int				m_video_input_kbps;		// 视频输入平均码流...

	int64_t			m_total_output_bytes;	// 总的输出字节数...
	int64_t			m_audio_output_bytes;	// 音频输出总字节数...
	int64_t			m_video_output_bytes;	// 视频输出总字节数...
	int				m_total_output_kbps;	// 总的输出平均码流...
	int				m_audio_output_kbps;	// 音频输出平均码率...
	int				m_video_output_kbps;	// 视频输出平均码流...
};