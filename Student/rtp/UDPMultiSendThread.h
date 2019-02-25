
#pragma once

#include <util/threading.h>
#include "HYDefine.h"
#include "OSThread.h"

class UDPSocket;
class CUDPRecvThread;
class CUDPMultiSendThread : public OSThread
{
public:
	CUDPMultiSendThread(CUDPRecvThread * lpServerThread);
	virtual ~CUDPMultiSendThread();
	virtual void Entry();
public:
	void            doResetMulticastIPSend();
	BOOL            InitThread(string & strHeader);
	BOOL            Transfer(char * lpBuffer, int inRecvLen);
	void            PushPacket(string & strPacket, uint8_t inPType);
private:
	BOOL            BuildDataSocket();
	BOOL            BuildLoseSocket();
	void            doSendHeaderCmd();
	void            ClearAllSocket();
	void			doRecvPacket();
	void            doSendLosePacket(uint8_t inPType, uint32_t inLoseSeq);
private:
	int64_t			 m_next_header_ns;		// 下次发送格式包的时间戳 => 纳秒 => 每隔1秒发送一次...
	string           m_strSeqHeader;        // 老师推流端上传的序列头命令包...
	circlebuf		 m_ex_audio_circle;		// 扩展音频环形队列
	circlebuf		 m_audio_circle;		// 音频环形队列
	circlebuf		 m_video_circle;		// 视频环形队列
	UDPSocket   *    m_lpUdpData;			// 组播数据发送套接字对象...
	UDPSocket   *    m_lpUdpLose;			// 组播丢包接收套接字对象...
	CUDPRecvThread * m_lpServerThread;		// 数据源服务器接收对象...
	pthread_mutex_t  m_MutexBuff;           // 互斥对象 => 保护环形队列...
	pthread_mutex_t  m_MutexSock;           // 互斥对象 => 保护组播套接字...
};
