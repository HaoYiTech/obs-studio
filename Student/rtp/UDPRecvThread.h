
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
		kCmdSendCreate	= 0,				// ��ʼ���� => ��������״̬
		kCmdConnectOK	= 1,				// ���뽻����ϣ��Ѿ���ʼ��������Ƶ������
	} m_nCmdState;							// ����״̬����...

	string			m_strSPS;				// ��Ƶsps
	string			m_strPPS;				// ��Ƶpps

	UDPSocket	*	m_lpUDPSocket;			// UDP����

	uint16_t		m_HostServerPort;		// �������˿� => host
	uint32_t	    m_HostServerAddr;		// ��������ַ => host
	string          m_HostServerStr;        // ��������ַ => string

	bool			m_bNeedSleep;			// ��Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
	int				m_dt_to_dir;			// ����·�߷��� => TO_SERVER | TO_P2P
	int				m_server_rtt_ms;		// Server => ���������ӳ�ֵ => ����
	int				m_server_rtt_var_ms;	// Server => ���綶��ʱ��� => ����
	int				m_server_cache_time_ms;	// Server => ��������ʱ��   => ���� => ���ǲ�����ʱʱ��
	int				m_nMaxResendCount;		// ��ǰ��������ط�����
	
	rtp_detect_t	m_rtp_detect;			// RTP̽������ṹ��
	rtp_create_t	m_rtp_create;			// RTP���������ֱ���ṹ��
	rtp_delete_t	m_rtp_delete;			// RTPɾ�������ֱ���ṹ��
	rtp_supply_t	m_rtp_supply;			// RTP��������ṹ��

	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ�� => ���� => ������ʦ������...
	string          m_strSeqHeader;         // ��ʦ�������ϴ�������ͷ�����...

	int64_t			m_sys_zero_ns;			// ϵͳ��ʱ��� => ��һ�����ݰ������ϵͳʱ�̵� => ����...
	int64_t			m_next_create_ns;		// �´η��ʹ�������ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_detect_ns;		// �´η���̽�����ʱ��� => ���� => ÿ��1�뷢��һ��...

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
	pthread_mutex_t m_MutexLose;            // �������Ᵽ������...

	CPlaySDL            * m_lpPlaySDL;           // SDL���Ź�����...
	CViewRender         * m_lpViewRender;        // ��Ⱦ���ڶ���...
	CUDPMultiSendThread * m_lpMultiSendThread;   // �鲥�������̶߳���...
};