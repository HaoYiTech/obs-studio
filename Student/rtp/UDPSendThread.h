
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
		kCmdSendCreate	= 0,				// ��ʼ���� => ��������״̬
		kCmdSendHeader	= 1,				// ��ʼ���� => ����ͷ����״̬
		kCmdWaitReady	= 2,				// �ȴ����� => ���Թۿ��˵�׼����������״̬
		kCmdSendAVPack	= 3,				// ��ʼ���� => ����Ƶ���ݰ�״̬
		kCmdUnkownState = 4,				// δ֪״̬ => �߳��Ѿ��˳�		
	} m_nCmdState;							// ����״̬����...

	string			m_strSPS;				// ��Ƶsps
	string			m_strPPS;				// ��Ƶpps

	OSMutex			m_Mutex;				// �������
	UDPSocket	*	m_lpUDPSocket;			// UDP����
	CDataThread *   m_lpDataThread;         // ��������

	uint16_t		m_HostServerPort;		// �������˿� => host
	uint32_t	    m_HostServerAddr;		// ��������ַ => host
	string          m_HostServerStr;        // ��������ַ => string

	bool			m_bNeedSleep;			// ��Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
	bool            m_bNeedDelete;			// ɾ����־ => û���û��������������ӵ��...
	int				m_dt_to_dir;			// ����·�߷��� => TO_SERVER | TO_P2P
	int				m_p2p_rtt_ms;			// P2P    => ���������ӳ�ֵ => ����
	int				m_p2p_rtt_var_ms;		// P2P    => ���綶��ʱ��� => ����
	int				m_server_rtt_ms;		// Server => ���������ӳ�ֵ => ����
	int				m_server_rtt_var_ms;	// Server => ���綶��ʱ��� => ����

	rtp_detect_t	m_rtp_detect;			// RTP̽������ṹ��
	rtp_create_t	m_rtp_create;			// RTP���������ֱ���ṹ��
	rtp_delete_t	m_rtp_delete;			// RTPɾ�������ֱ���ṹ��
	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ��

	rtp_ready_t		m_rtp_ready;			// RTP׼�������ṹ�� => ���� => ����ۿ�����Ϣ

	int64_t			m_next_create_ns;		// �´η��ʹ�������ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_header_ns;		// �´η�������ͷ����ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_detect_ns;		// �´η���̽�����ʱ��� => ���� => ÿ��1�뷢��һ��...

	circlebuf		m_audio_circle;			// ��Ƶ���ζ���
	circlebuf		m_video_circle;			// ��Ƶ���ζ���

	uint32_t		m_nAudioCurPackSeq;		// ��ƵRTP��ǰ������к�
	uint32_t		m_nAudioCurSendSeq;		// ��ƵRTP��ǰ�������к�

	uint32_t		m_nVideoCurPackSeq;		// ��ƵRTP��ǰ������к�
	uint32_t		m_nVideoCurSendSeq;		// ��ƵRTP��ǰ�������к�

	GM_MapLose		m_AudioMapLose;			// ��Ƶ��⵽�Ķ������϶���...
	GM_MapLose		m_VideoMapLose;			// ��Ƶ��⵽�Ķ������϶���...

	int64_t			m_start_time_ns;		// ������������ʱ��...
	int64_t			m_total_time_ms;		// �ܵĳ���������...

	int64_t			m_audio_input_bytes;	// ��Ƶ�������ֽ���...
	int64_t			m_video_input_bytes;	// ��Ƶ�������ֽ���...
	int				m_audio_input_kbps;		// ��Ƶ����ƽ������...
	int				m_video_input_kbps;		// ��Ƶ����ƽ������...

	int64_t			m_total_output_bytes;	// �ܵ�����ֽ���...
	int64_t			m_audio_output_bytes;	// ��Ƶ������ֽ���...
	int64_t			m_video_output_bytes;	// ��Ƶ������ֽ���...
	int				m_total_output_kbps;	// �ܵ����ƽ������...
	int				m_audio_output_kbps;	// ��Ƶ���ƽ������...
	int				m_video_output_kbps;	// ��Ƶ���ƽ������...
};