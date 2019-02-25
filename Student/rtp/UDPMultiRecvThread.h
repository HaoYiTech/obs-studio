
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
	void            doDeleteExAudioThread();
private:
	void            ResetExAudio();
	void            ClosePlayer();
	void            ClearAllSocket();
	BOOL            BuildDataSocket();
	BOOL            BuildLoseSocket();

	void			doSendSupplyCmd(uint8_t inPType);
	void            doCheckRecvTimeout();
	void			doRecvPacket();
	void			doSleepTo();

	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);

	void			doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID);
	void			doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID);
	void			doParseFrame(uint8_t inPType);

	void			doServerMinSeq(uint8_t inPType, uint32_t inMinSeq);
private:
	enum {
		kCmdSendCreate = 0,					// ��ʼ���� => ��������״̬
		kCmdConnetOK = 1,					// ���뽻����ϣ�ֻ����Ϊһ����ֹ��������׼����������ı�־
	} m_nCmdState;							// ����״̬����...

	string			m_strSPS;				// ��Ƶsps
	string			m_strPPS;				// ��Ƶpps

	bool			m_bNeedSleep;			// ��Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
	int64_t			m_time_zero_ns;			// ��ʱ0��ʱ�� => ����...
	int64_t			m_sys_zero_ns;			// ϵͳ��ʱ��� => ��һ�����ݰ������ϵͳʱ�̵� => ����...
	int				m_server_rtt_ms;		// Server => ���������ӳ�ֵ => ����
	int				m_server_rtt_var_ms;	// Server => ���綶��ʱ��� => ����
	int				m_server_cache_time_ms;	// Server => ��������ʱ��   => ���� => ���ǲ�����ʱʱ��
	int				m_nMaxResendCount;		// ��ǰ��������ط�����

	rtp_supply_t	m_rtp_supply;			// RTP��������ṹ��
	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ�� => ���� => ������ʦ������...
	string          m_strSeqHeader;         // ��ʦ�������ϴ�������ͷ�����...

	circlebuf		m_audio_circle;			// ��Ƶ���ζ���
	circlebuf		m_video_circle;			// ��Ƶ���ζ���

	bool			m_bFirstAudioSeq;		// ��Ƶ��һ�����ݰ����յ���־...
	bool			m_bFirstVideoSeq;		// ��Ƶ��һ�����ݰ����յ���־...
	uint32_t		m_nAudioMaxPlaySeq;		// ��ƵRTP��ǰ��󲥷����к� => ���������Ч���к�...
	uint32_t		m_nVideoMaxPlaySeq;		// ��ƵRTP��ǰ��󲥷����к� => ���������Ч���к�...

	bool            m_Ex_bFirstAudioSeq;    // ��չ��Ƶ��һ�����ݰ����յ���־...
	uint16_t        m_Ex_wAudioChangeNum;   // ��չ��Ƶ�仯����...
	uint32_t        m_Ex_nAudioMaxPlaySeq;  // ��չ��ƵRTP��ǰ��󲥷����к� => ���������Ч���к�...
	GM_MapLose		m_Ex_AudioMapLose;		// ��չ��Ƶ��⵽�Ķ������϶���...
	circlebuf		m_Ex_audio_circle;		// ��չ��Ƶ���ζ���

	GM_MapLose		m_AudioMapLose;			// ��Ƶ��⵽�Ķ������϶���...
	GM_MapLose		m_VideoMapLose;			// ��Ƶ��⵽�Ķ������϶���...

	UDPSocket   *   m_lpUdpData;			// �鲥���ݽ����׽��ֶ���...
	UDPSocket   *   m_lpUdpLose;			// �鲥���������׽��ֶ���...
	CPlaySDL    *   m_lpPlaySDL;            // SDL���Ź�����...
	CViewRender *   m_lpViewRender;         // ��Ⱦ���ڶ���...
};
