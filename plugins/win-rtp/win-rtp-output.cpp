
#include <obs-avc.h>
#include <obs-module.h>
#include "UDPSendThread.h"

OSMutex  out_mutex;  // 输出互斥...

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
	// 获取传递过来的输出对象和配置信息...
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	lpRtpStream->room_id = (int)obs_data_get_int(settings, "room_id");
	lpRtpStream->udp_addr = obs_data_get_string(settings, "udp_addr");
	lpRtpStream->udp_port = (int)obs_data_get_int(settings, "udp_port");
	lpRtpStream->tcp_socket = (int)obs_data_get_int(settings, "tcp_socket");
	// 如果推流线程已经创建，直接删除...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
	ASSERT(lpRtpStream->sendThread == NULL);
	// 判断房间信息和传递参数是否有效，无效直接返回...
	if (lpRtpStream->room_id <= 0 || lpRtpStream->tcp_socket <= 0 ||
		lpRtpStream->udp_port <= 0 || lpRtpStream->udp_addr == NULL) {
		blog(LOG_INFO, "%s rtp_output_update => Failed, RoomID: %d, TCPSock: %d, %s:%d", TM_SEND_NAME,
			lpRtpStream->room_id, lpRtpStream->tcp_socket, lpRtpStream->udp_addr, lpRtpStream->udp_port);
		return;
	}
	// 创建新的推流线程对象，并初始化，将推流线程对象保存到输出管理器当中...
	lpRtpStream->sendThread = new CUDPSendThread(lpRtpStream->room_id, lpRtpStream->tcp_socket);
	// 打印成功创建接收线程信息 => room_id | udp_addr | udp_port | tcp_socket
	blog(LOG_INFO, "%s rtp_source_update => Success, RoomID: %d, TCPSock: %d, %s:%d", TM_SEND_NAME,
		lpRtpStream->room_id, lpRtpStream->tcp_socket, lpRtpStream->udp_addr, lpRtpStream->udp_port);
}

static void *rtp_output_create(obs_data_t *settings, obs_output_t *output)
{
	OSMutexLocker theLock(&out_mutex);
	// 创建rtp数据源变量管理对象...
	win_rtp_output * lpRtpStream = (win_rtp_output*)bzalloc(sizeof(struct win_rtp_output));
	// 将传递过来的obs输出对象保存起来...
	lpRtpStream->output = output;
	// 先调用配置发生变化的处理...
	rtp_output_update(lpRtpStream, settings);
	// 返回输出变量管理对象...
	return lpRtpStream;
}

static void rtp_output_destroy(void *data)
{
	OSMutexLocker theLock(&out_mutex);
	// 注意：不能再次调用 obs_output_end_data_capture，可能会引起压缩器崩溃...
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// 如果推流线程被创建过，需要被删除...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
	// 释放rtp输出管理器...
	bfree(lpRtpStream);
}

static bool rtp_output_start(void *data)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// 判断上层是否可以进行数据捕捉 => 查询失败，打印错误信息...
	if (!obs_output_can_begin_data_capture(lpRtpStream->output, 0)) {
		blog(LOG_INFO, "%s begin capture failed!", TM_SEND_NAME);
		return false;
	}
	// 通知上层初始化压缩器 => 启动失败，打印错误信息...
	if (!obs_output_initialize_encoders(lpRtpStream->output, 0)) {
		blog(LOG_INFO, "%s init encoder failed!", TM_SEND_NAME);
		return false;
	}
	// 如果推流线程无效，直接返回...
	if (lpRtpStream->sendThread == NULL) {
		blog(LOG_INFO, "%s send thread failed!", TM_SEND_NAME);
		return false;
	}
	// 用音视频格式头初始化推流线程 => 初始化失败，直接返回...
	if (!lpRtpStream->sendThread->StartThread(lpRtpStream->output, lpRtpStream->udp_addr, lpRtpStream->udp_port)) {
		blog(LOG_INFO, "%s start thread failed!", TM_SEND_NAME);
		return false;
	}
	// 先不要启动数据采集压缩过程，等推流线程准备好之后，再启动，以免数据丢失，造成启动延时...
	// CUDPSendThread::doProcServerHeader => 推流线程连接服务器成功之后，才启动数据采集压缩...
	return true;
}

static void rtp_output_stop(void *data, uint64_t ts)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// OBS_OUTPUT_SUCCESS 会调用 obs_output_end_data_capture...
	obs_output_signal_stop(lpRtpStream->output, OBS_OUTPUT_SUCCESS);
	// 上层停止产生数据之后，删除推流线程对象...
	if (lpRtpStream->sendThread != NULL) {
		delete lpRtpStream->sendThread;
		lpRtpStream->sendThread = NULL;
	}
}

static void rtp_output_data(void *data, struct encoder_packet *packet)
{
	OSMutexLocker theLock(&out_mutex);
	win_rtp_output * lpRtpStream = (win_rtp_output*)data;
	// 将数据帧投递给推流线程 => 投递原始数据，不用做任何处理...
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
