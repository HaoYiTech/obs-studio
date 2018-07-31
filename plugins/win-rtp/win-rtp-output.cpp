
#include <obs-avc.h>
#include <obs-module.h>
#include "UDPSendThread.h"

OSMutex  out_mutex;  // �������...

struct win_rtp_output {
	obs_output_t   * output;
	int              room_id;
	const char     * udp_addr;
	int              udp_port;
	int              tcp_socket;
	CUDPSendThread * sendThread;
};

static const char *rtp_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RTPOutput");
}

static void rtp_output_update(void *data, obs_data_t *settings)
{
	OSMutexLocker theLock(&out_mutex);
	// ��ȡ���ݹ�������������������Ϣ...
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	lpRtpStream->room_id = (int)obs_data_get_int(settings, "room_id");
	lpRtpStream->udp_addr = obs_data_get_string(settings, "udp_addr");
	lpRtpStream->udp_port = (int)obs_data_get_int(settings, "udp_port");
	lpRtpStream->tcp_socket = (int)obs_data_get_int(settings, "tcp_socket");
	// ��������߳��Ѿ�������ֱ��ɾ��...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
	ASSERT(lpRtpStream->sendThread == NULL);
	// �жϷ�����Ϣ�ʹ��ݲ����Ƿ���Ч����Чֱ�ӷ���...
	if (lpRtpStream->room_id <= 0 || lpRtpStream->tcp_socket <= 0 ||
		lpRtpStream->udp_port <= 0 || lpRtpStream->udp_addr == NULL) {
		blog(LOG_INFO, "%s rtp_output_update => Failed, RoomID: %d, TCPSock: %d, %s:%d", TM_SEND_NAME,
			lpRtpStream->room_id, lpRtpStream->tcp_socket, lpRtpStream->udp_addr, lpRtpStream->udp_port);
		return;
	}
	// �����µ������̶߳��󣬲���ʼ�����������̶߳��󱣴浽�������������...
	lpRtpStream->sendThread = new CUDPSendThread(lpRtpStream->room_id, lpRtpStream->tcp_socket);
	// ��ӡ�ɹ����������߳���Ϣ => room_id | udp_addr | udp_port | tcp_socket
	blog(LOG_INFO, "%s rtp_source_update => Success, RoomID: %d, TCPSock: %d, %s:%d", TM_SEND_NAME,
		lpRtpStream->room_id, lpRtpStream->tcp_socket, lpRtpStream->udp_addr, lpRtpStream->udp_port);
}

static void *rtp_output_create(obs_data_t *settings, obs_output_t *output)
{
	OSMutexLocker theLock(&out_mutex);
	// ����rtp����Դ�����������...
	win_rtp_output * lpRtpStream = (win_rtp_output*)bzalloc(sizeof(struct win_rtp_output));
	// �����ݹ�����obs������󱣴�����...
	lpRtpStream->output = output;
	// �ȵ������÷����仯�Ĵ���...
	rtp_output_update(lpRtpStream, settings);
	// ������������������...
	return lpRtpStream;
}

static void rtp_output_destroy(void *data)
{
	OSMutexLocker theLock(&out_mutex);
	// ע�⣺�����ٴε��� obs_output_end_data_capture�����ܻ�����ѹ��������...
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// ��������̱߳�����������Ҫ��ɾ��...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
	// �ͷ�rtp���������...
	bfree(lpRtpStream);
}

static bool rtp_output_start(void *data)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// �ж��ϲ��Ƿ���Խ������ݲ�׽ => ��ѯʧ�ܣ���ӡ������Ϣ...
	if (!obs_output_can_begin_data_capture(lpRtpStream->output, 0)) {
		blog(LOG_INFO, "%s begin capture failed!", TM_SEND_NAME);
		return false;
	}
	// ֪ͨ�ϲ��ʼ��ѹ���� => ����ʧ�ܣ���ӡ������Ϣ...
	if (!obs_output_initialize_encoders(lpRtpStream->output, 0)) {
		blog(LOG_INFO, "%s init encoder failed!", TM_SEND_NAME);
		return false;
	}
	// ��������߳���Ч��ֱ�ӷ���...
	if (lpRtpStream->sendThread == NULL) {
		blog(LOG_INFO, "%s send thread failed!", TM_SEND_NAME);
		return false;
	}
	// ������Ƶ��ʽͷ��ʼ�������߳� => ��ʼ��ʧ�ܣ�ֱ�ӷ���...
	if (!lpRtpStream->sendThread->StartThread(lpRtpStream->output, lpRtpStream->udp_addr, lpRtpStream->udp_port)) {
		blog(LOG_INFO, "%s start thread failed!", TM_SEND_NAME);
		return false;
	}
	// �Ȳ�Ҫ�������ݲɼ�ѹ�����̣��������߳�׼����֮�����������������ݶ�ʧ�����������ʱ...
	// CUDPSendThread::doProcServerHeader => �����߳����ӷ������ɹ�֮�󣬲��������ݲɼ�ѹ��...
	return true;
}

static void rtp_output_stop(void *data, uint64_t ts)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// OBS_OUTPUT_SUCCESS ����� obs_output_end_data_capture...
	obs_output_signal_stop(lpRtpStream->output, OBS_OUTPUT_SUCCESS);
	// �ϲ�ֹͣ��������֮��ɾ�������̶߳���...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
}

static void rtp_output_data(void *data, struct encoder_packet *packet)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// ������֡Ͷ�ݸ������߳� => Ͷ��ԭʼ���ݣ��������κδ���...
	if (lpRtpStream->sendThread != NULL) {
		lpRtpStream->sendThread->PushFrame(packet);
	}
}

static void rtp_output_defaults(obs_data_t *defaults)
{
}

static obs_properties_t *rtp_output_properties(void *unused)
{
	return NULL;
}

static uint64_t rtp_output_total_bytes_sent(void *data)
{
	return 0;
}

static int rtp_output_connect_time(void *data)
{
	return 0;
}

static int rtp_output_dropped_frames(void *data)
{
	return 0;
}

void RegisterWinRtpOutput()
{
	obs_output_info rtp_output = {};
	rtp_output.id = "rtp_output";
	rtp_output.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED;
	rtp_output.encoded_video_codecs = "h264";
	rtp_output.encoded_audio_codecs = "aac";
	rtp_output.get_name = rtp_output_getname;
	rtp_output.create = rtp_output_create;
	rtp_output.update = rtp_output_update;
	rtp_output.destroy = rtp_output_destroy;
	rtp_output.start = rtp_output_start;
	rtp_output.stop = rtp_output_stop;
	rtp_output.encoded_packet = rtp_output_data;
	rtp_output.get_defaults = rtp_output_defaults;
	rtp_output.get_properties = rtp_output_properties;
	rtp_output.get_total_bytes = rtp_output_total_bytes_sent;
	rtp_output.get_connect_time_ms = rtp_output_connect_time;
	rtp_output.get_dropped_frames = rtp_output_dropped_frames;
	obs_register_output(&rtp_output);
}
