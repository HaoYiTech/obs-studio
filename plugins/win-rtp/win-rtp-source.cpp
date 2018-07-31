
#include <obs-module.h>
#include "UDPRecvThread.h"

struct win_rtp_source {
	obs_source_t   * source;
	CUDPRecvThread * recvThread;
};

static const char *rtp_source_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RTPSource");
}

static void rtp_source_update(void *data, obs_data_t *settings)
{
	// 获取传递过来的资源对象和配置信息...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBLiveID = (int)obs_data_get_int(settings, "camera_id");
	// 如果房间编号和摄像头编号无效，删除接收线程对象...
	if (nDBRoomID <= 0 || nDBLiveID <= 0) {
		// 如果已经创建过接收线程，删除这个对象...
		if( lpRtpSource->recvThread != NULL ) {
			delete lpRtpSource->recvThread;
			lpRtpSource->recvThread = NULL;
		}
		// 没有房间号或没有摄像头，直接返回...
		blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Failed, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
		return;
	}
	// 如果接收线程已经创建，房间号和摄像头编号没有发生变化，直接返回...
	CUDPRecvThread * lpRecvThread = lpRtpSource->recvThread;
	int nOldRoomID = ((lpRecvThread == NULL) ? -1 : lpRecvThread->GetRoomID());
	int nOldLiveID = ((lpRecvThread == NULL) ? -1 : lpRecvThread->GetLiveID());
	if((lpRecvThread != NULL) && (nOldRoomID == nDBRoomID) && (nOldLiveID == nDBLiveID)) {
		blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Origin, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
		return;
	}
	// 如果接收线程已经创建，但是，房间号或摄像头编号，发生变化，删除接收线程...
	if( lpRtpSource->recvThread != NULL ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
	}
	ASSERT(lpRtpSource->recvThread == NULL);
	// 创建新的观看线程对象，并初始化，将接收线程对象保存到数据源管理器当中...
	lpRtpSource->recvThread = new CUDPRecvThread(nDBRoomID, nDBLiveID);
	lpRtpSource->recvThread->InitThread(lpRtpSource->source);
	// 打印成功创建接收线程信息...
	blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Success, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
	// 让播放层不要对数据帧进行缓存，直接最快播放...
	obs_source_set_async_unbuffered(lpRtpSource->source, true);
	//obs_source_set_async_decoupled(lpRtpSource->source, true);
}

static void *rtp_source_create(obs_data_t *settings, obs_source_t *source)
{
	// 创建rtp数据源变量管理对象...
	win_rtp_source * lpRtpSource = (win_rtp_source*)bzalloc(sizeof(struct win_rtp_source));
	// 将传递过来的obs资源对象保存起来...
	lpRtpSource->source = source;
	// 先调用配置发生变化的处理...
	rtp_source_update(lpRtpSource, settings);
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
	if (lpRtpSource->recvThread == NULL)
		return;
	// 如果已经发生登录超时，删除接收线程...
	if( lpRtpSource->recvThread->IsLoginTimeout() ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		return;
	}
	// 如果已经发生系统重载，删除并重建接收线程...
	/*if( lpRtpSource->recvThread->IsReloadFlag() ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
	}*/
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
	obs_register_source(&rtp_source);
}
