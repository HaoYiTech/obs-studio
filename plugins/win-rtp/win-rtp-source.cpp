
#include <obs-module.h>
#include "UDPRecvThread.h"

struct win_rtp_source {
	obs_source_t   * source;
	CUDPRecvThread * recvThread;
};

// 拉流线程是否有效标志...
bool g_HasRecvThread = false;

static const char *rtp_source_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RTPSource");
}

// 处理摄像头通道上线通知...
static void doCameraOnLine(void *data, obs_data_t *settings)
{
	// 获取传递过来的资源对象和配置信息...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	const char * lpUdpAddr = obs_data_get_string(settings, "udp_addr");
	int nUdpPort = (int)obs_data_get_int(settings, "udp_port");
	int nTCPSockFD = (int)obs_data_get_int(settings, "tcp_socket");
	// 如果是摄像头通道上线通知 => 如果拉流对象不为空，删除拉流对象...
	if (lpRtpSource->recvThread != NULL) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// 将画面绘制过程设置为非活动状态，等待新的画面到达...
		obs_source_output_video(lpRtpSource->source, NULL);
	}
	// 设置拉流线程未创建标志...
	g_HasRecvThread = false;
	// 房间号必须有效、摄像头编号必须有效、摄像头必须在线、套接字必须有效、地址必须有效...
	if (nDBRoomID <= 0 || nDBCameraID <= 0 || nTCPSockFD <= 0 || lpUdpAddr == NULL || nUdpPort <= 0) {
		blog(LOG_INFO, "%s rtp_source_update => OnLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
			TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
		return;
	}
	// 使用验证过的有效参数，重建拉流对象，并初始化，将接收线程对象保存到数据源管理器当中...
	lpRtpSource->recvThread = new CUDPRecvThread(nTCPSockFD, nDBRoomID, nDBCameraID);
	lpRtpSource->recvThread->InitThread(lpRtpSource->source, lpUdpAddr, nUdpPort);
	// 让播放层不要对数据帧进行缓存，直接最快播放 => 这个很重要，可以有效降低播放延时...
	obs_source_set_async_unbuffered(lpRtpSource->source, true);
	//obs_source_set_async_decoupled(lpRtpSource->source, true);
	// 设置拉流线程已创建标志...
	g_HasRecvThread = true;
	// 打印成功创建接收线程信息 => 将所有传入的参数打印出来...
	blog(LOG_INFO, "%s rtp_source_update => OnLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
		TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
}

// 处理摄像头通道下线通知...
static void doCameraOffLine(void *data, obs_data_t *settings)
{
	// 获取传递过来的资源对象和配置信息...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	const char * lpUdpAddr = obs_data_get_string(settings, "udp_addr");
	int nUdpPort = (int)obs_data_get_int(settings, "udp_port");
	int nTCPSockFD = (int)obs_data_get_int(settings, "tcp_socket");
	// 进行单次循环验证处理过程...
	do {
		// 如果是摄像头通道下线通知 => 拉流对象为空，直接返回...
		if (lpRtpSource->recvThread == NULL) {
			g_HasRecvThread = false;
			break;
		}
		ASSERT(lpRtpSource->recvThread != NULL);
		// 如果拉流对象不为空，但通道编号不一致，不能删除，直接返回...
		if (lpRtpSource->recvThread->GetDBCameraID() != nDBCameraID) {
			g_HasRecvThread = true;
			break;
		}
		// 如果拉流对象不为空，通道编号一致，删除拉流对象...
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// 将画面绘制过程设置为非活动状态，等待新的画面到达...
		obs_source_output_video(lpRtpSource->source, NULL);
		// 设置拉流线程未创建标志...
		g_HasRecvThread = false;
	} while (false);
	// 打印成功删除拉流线程信息 => 将所有传入的参数打印出来...
	blog(LOG_INFO, "%s rtp_source_update => OffLine, HasRecv: %d, RoomID: %d, CameraID: %d, TCPSock: %d, %s:%d",
		TM_RECV_NAME, g_HasRecvThread, nDBRoomID, nDBCameraID, nTCPSockFD, lpUdpAddr, nUdpPort);
}

// 处理rtp_source资源配置发生变化的事件通知...
static void rtp_source_update(void *data, obs_data_t *settings)
{
	int nDBCameraID = (int)obs_data_get_int(settings, "camera_id");
	bool bIsCameraOnLine = obs_data_get_bool(settings, "udp_camera");
	// 根据摄像头通道的上线通知或下线通知来进行单独分开处理...
	bIsCameraOnLine ? doCameraOnLine(data, settings) : doCameraOffLine(data, settings);
}

// 注意：创建过程不要进行拉流对象的创建，而是通过上层调用rtp_source_update来完成拉流线程创建...
static void *rtp_source_create(obs_data_t *settings, obs_source_t *source)
{
	// 创建rtp数据源变量管理对象...
	win_rtp_source * lpRtpSource = (win_rtp_source*)bzalloc(sizeof(struct win_rtp_source));
	// 将传递过来的obs资源对象保存起来...
	lpRtpSource->source = source;
	// 设置拉流线程未创建标志...
	g_HasRecvThread = false;
	// 返回资源变量管理对象...
	return lpRtpSource;
}

static void rtp_source_destroy(void *data)
{
	// 如果接收线程被创建过，需要被删除...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	if (lpRtpSource->recvThread != NULL) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
	}
	// 设置拉流线程未创建标志...
	g_HasRecvThread = false;
	// 释放rtp资源管理器...
	bfree(lpRtpSource);
}

static void rtp_source_defaults(obs_data_t *settings)
{
	//obs_data_set_default_int(settings, "room_id", 0);
	//obs_data_set_default_int(settings, "camera_id", 0);
}

// 使用列表框展示在线的摄像头列表，无需属性配置...
static obs_properties_t *rtp_source_properties(void *data)
{
	return NULL;
}

// 上层系统传递过来的心跳过程...
static void rtp_source_tick(void *data, float seconds)
{
	// 接收线程如果无效，直接返回...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	// 设置拉流线程未创建标志...
	if (lpRtpSource->recvThread == NULL) {
		g_HasRecvThread = false;
		return;
	}
	// 如果已经发生登录超时，删除接收线程...
	if( lpRtpSource->recvThread->IsLoginTimeout() ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		// 设置拉流线程未创建标志...
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
