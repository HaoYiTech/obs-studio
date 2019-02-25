
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
	int64_t			 m_next_header_ns;		// �´η��͸�ʽ����ʱ��� => ���� => ÿ��1�뷢��һ��...
	string           m_strSeqHeader;        // ��ʦ�������ϴ�������ͷ�����...
	circlebuf		 m_ex_audio_circle;		// ��չ��Ƶ���ζ���
	circlebuf		 m_audio_circle;		// ��Ƶ���ζ���
	circlebuf		 m_video_circle;		// ��Ƶ���ζ���
	UDPSocket   *    m_lpUdpData;			// �鲥���ݷ����׽��ֶ���...
	UDPSocket   *    m_lpUdpLose;			// �鲥���������׽��ֶ���...
	CUDPRecvThread * m_lpServerThread;		// ����Դ���������ն���...
	pthread_mutex_t  m_MutexBuff;           // ������� => �������ζ���...
	pthread_mutex_t  m_MutexSock;           // ������� => �����鲥�׽���...
};
