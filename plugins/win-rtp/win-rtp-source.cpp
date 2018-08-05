
#include <obs-module.h>
#include "UDPRecvThread.h"

struct win_rtp_source {
	obs_source_t   * source;
	CUDPRecvThread * recvThread;
};

// �����߳��Ƿ���Ч��־...
bool g_HasRecvThread = false;

static const char *rtp_source_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RTPSource");
}

// ��������ͷͨ������֪ͨ...
static void doCameraOnLine(void *data, obs_data_t *settings)
{
	// ��ȡ���ݹ�������Դ�����������Ϣ...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	const char * lpUdpAddr = obs_data_get_string(settings, "udp_addr");
	int nUdpPort = (int)obs_data_get_int(settings, "udp_port");
	int nTCPSockFD = (int)obs_data_get_int(settings, "tcp_socket");
	// ���������ͷͨ������֪ͨ => �����������Ϊ�գ�ɾ����������...
	if (lpRtpSource->recvThread != NULL) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// ��������ƹ�������Ϊ�ǻ״̬���ȴ��µĻ��浽��...
		obs_source_output_video(lpRtpSource->source, NULL);
	}
	// ���������߳�δ������־...
	g_HasRecvThread = false;
	// ����ű�����Ч������ͷ��ű�����Ч������ͷ�������ߡ��׽��ֱ�����Ч����ַ������Ч...
	if (nDBRoomID <= 0 || nDBCameraID <= 0 || nTCPSockFD <= 0 || lpUdpAddr == NULL || nUdpPort <= 0) {
		blog(LOG_INFO, "%s rtp_source_update => OnLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
			TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
		return;
	}
	// ʹ����֤������Ч�������ؽ��������󣬲���ʼ�����������̶߳��󱣴浽����Դ����������...
	lpRtpSource->recvThread = new CUDPRecvThread(nTCPSockFD, nDBRoomID, nDBCameraID);
	lpRtpSource->recvThread->InitThread(lpRtpSource->source, lpUdpAddr, nUdpPort);
	// �ò��Ų㲻Ҫ������֡���л��棬ֱ����첥�� => �������Ҫ��������Ч���Ͳ�����ʱ...
	obs_source_set_async_unbuffered(lpRtpSource->source, true);
	//obs_source_set_async_decoupled(lpRtpSource->source, true);
	// ���������߳��Ѵ�����־...
	g_HasRecvThread = true;
	// ��ӡ�ɹ����������߳���Ϣ => �����д���Ĳ�����ӡ����...
	blog(LOG_INFO, "%s rtp_source_update => OnLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
		TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
}

// ��������ͷͨ������֪ͨ...
static void doCameraOffLine(void *data, obs_data_t *settings)
{
	// ��ȡ���ݹ�������Դ�����������Ϣ...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	const char * lpUdpAddr = obs_data_get_string(settings, "udp_addr");
	int nUdpPort = (int)obs_data_get_int(settings, "udp_port");
	int nTCPSockFD = (int)obs_data_get_int(settings, "tcp_socket");
	// ���е���ѭ����֤�������...
	do {
		// ���������ͷͨ������֪ͨ => ��������Ϊ�գ�ֱ�ӷ���...
		if (lpRtpSource->recvThread == NULL) {
			g_HasRecvThread = false;
			break;
		}
		ASSERT(lpRtpSource->recvThread != NULL);
		// �����������Ϊ�գ���ͨ����Ų�һ�£�����ɾ����ֱ�ӷ���...
		if (lpRtpSource->recvThread->GetDBCameraID() != nDBCameraID) {
			g_HasRecvThread = true;
			break;
		}
		// �����������Ϊ�գ�ͨ�����һ�£�ɾ����������...
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// ��������ƹ�������Ϊ�ǻ״̬���ȴ��µĻ��浽��...
		obs_source_output_video(lpRtpSource->source, NULL);
		// ���������߳�δ������־...
		g_HasRecvThread = false;
	} while (false);
	// ��ӡ�ɹ�ɾ�������߳���Ϣ => �����д���Ĳ�����ӡ����...
	blog(LOG_INFO, "%s rtp_source_update => OffLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
		TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
}

// ����rtp_source��Դ���÷����仯���¼�֪ͨ...
static void rtp_source_update(void *data, obs_data_t *settings)
{
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	// ��������ͷͨ��������֪ͨ������֪ͨ�����е����ֿ�����...
	bIsCameraOnLine ? doCameraOnLine(data, settings) : doCameraOffLine(data, settings);
}

// ע�⣺�������̲�Ҫ������������Ĵ���������ͨ���ϲ����rtp_source_update����������̴߳���...
static void *rtp_source_create(obs_data_t *settings, obs_source_t *source)
{
	// ����rtp����Դ�����������...
	win_rtp_source * lpRtpSource = (win_rtp_source*)bzalloc(sizeof(struct win_rtp_source));
	// �����ݹ�����obs��Դ���󱣴�����...
	lpRtpSource->source = source;
	// ���������߳�δ������־...
	g_HasRecvThread = false;
	// ������Դ�����������...
	return lpRtpSource;
}

static void rtp_source_destroy(void *data)
{
	// ��������̱߳�����������Ҫ��ɾ��...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	if (lpRtpSource->recvThread != NULL) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
	}
	// ���������߳�δ������־...
	g_HasRecvThread = false;
	// �ͷ�rtp��Դ������...
	bfree(lpRtpSource);
}

static void rtp_source_defaults(obs_data_t *settings)
{
	//obs_data_set_default_int(settings, "room_id", 0);
	//obs_data_set_default_int(settings, "camera_id", 0);
}

// ʹ���б��չʾ���ߵ�����ͷ�б�������������...
static obs_properties_t *rtp_source_properties(void *data)
{
	return NULL;
}

// �ϲ�ϵͳ���ݹ�������������...
static void rtp_source_tick(void *data, float seconds)
{
	// �����߳������Ч��ֱ�ӷ���...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	// ���������߳�δ������־...
	if (lpRtpSource->recvThread == NULL) {
		g_HasRecvThread = false;
		return;
	}
	// ����Ѿ�������¼��ʱ��ɾ�������߳�...
	if( lpRtpSource->recvThread->IsLoginTimeout() ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// ���������߳�δ������־...
		g_HasRecvThread = false;
		return;
	}
}

static void rtp_source_activate(void *data)
{
}

static void rtp_source_deactivate(void *data)
{
}

void RegisterWinRtpSource()
{
	obs_source_info rtp_source = {};
	rtp_source.id              = "rtp_source";
	rtp_source.type            = OBS_SOURCE_TYPE_INPUT;
	rtp_source.output_flags    = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	rtp_source.get_name        = rtp_source_getname;
	rtp_source.create          = rtp_source_create;
	rtp_source.update          = rtp_source_update;
	rtp_source.destroy         = rtp_source_destroy;
	rtp_source.get_defaults    = rtp_source_defaults;
	rtp_source.get_properties  = rtp_source_properties;
	rtp_source.activate        = rtp_source_activate;
	rtp_source.deactivate      = rtp_source_deactivate;
	rtp_source.video_tick      = rtp_source_tick;
	rtp_source.type_data       = (void*)&g_HasRecvThread;
	obs_register_source(&rtp_source);
}
