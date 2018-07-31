
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
	// ��ȡ���ݹ�������Դ�����������Ϣ...
	win_rtp_source * lpRtpSource = (win_rtp_source*)data;
	int nDBRoomID = (int)obs_data_get_int(settings, "room_id");
	int nDBLiveID = (int)obs_data_get_int(settings, "camera_id");
	// ��������ź�����ͷ�����Ч��ɾ�������̶߳���...
	if (nDBRoomID <= 0 || nDBLiveID <= 0) {
		// ����Ѿ������������̣߳�ɾ���������...
		if( lpRtpSource->recvThread != NULL ) {
			delete lpRtpSource->recvThread;
			lpRtpSource->recvThread = NULL;
		}
		// û�з���Ż�û������ͷ��ֱ�ӷ���...
		blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Failed, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
		return;
	}
	// ��������߳��Ѿ�����������ź�����ͷ���û�з����仯��ֱ�ӷ���...
	CUDPRecvThread * lpRecvThread = lpRtpSource->recvThread;
	int nOldRoomID = ((lpRecvThread == NULL) ? -1 : lpRecvThread->GetRoomID());
	int nOldLiveID = ((lpRecvThread == NULL) ? -1 : lpRecvThread->GetLiveID());
	if((lpRecvThread != NULL) && (nOldRoomID == nDBRoomID) && (nOldLiveID == nDBLiveID)) {
		blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Origin, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
		return;
	}
	// ��������߳��Ѿ����������ǣ�����Ż�����ͷ��ţ������仯��ɾ�������߳�...
	if( lpRtpSource->recvThread != NULL ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
	}
	ASSERT(lpRtpSource->recvThread == NULL);
	// �����µĹۿ��̶߳��󣬲���ʼ�����������̶߳��󱣴浽����Դ����������...
	lpRtpSource->recvThread = new CUDPRecvThread(nDBRoomID, nDBLiveID);
	lpRtpSource->recvThread->InitThread(lpRtpSource->source);
	// ��ӡ�ɹ����������߳���Ϣ...
	blog(LOG_INFO, "[Teacher-Looker] rtp_source_update => Success, RoomID: %d, LiveID: %d", nDBRoomID, nDBLiveID);
	// �ò��Ų㲻Ҫ������֡���л��棬ֱ����첥��...
	obs_source_set_async_unbuffered(lpRtpSource->source, true);
	//obs_source_set_async_decoupled(lpRtpSource->source, true);
}

static void *rtp_source_create(obs_data_t *settings, obs_source_t *source)
{
	// ����rtp����Դ�����������...
	win_rtp_source * lpRtpSource = (win_rtp_source*)bzalloc(sizeof(struct win_rtp_source));
	// �����ݹ�����obs��Դ���󱣴�����...
	lpRtpSource->source = source;
	// �ȵ������÷����仯�Ĵ���...
	rtp_source_update(lpRtpSource, settings);
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
	if (lpRtpSource->recvThread == NULL)
		return;
	// ����Ѿ�������¼��ʱ��ɾ�������߳�...
	if( lpRtpSource->recvThread->IsLoginTimeout() ) {
		delete lpRtpSource->recvThread;
		lpRtpSource->recvThread = NULL;
		return;
	}
	// ����Ѿ�����ϵͳ���أ�ɾ�����ؽ������߳�...
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
