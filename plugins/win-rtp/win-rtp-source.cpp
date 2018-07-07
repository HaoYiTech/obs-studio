
#include <obs-module.h>

static const char *rtp_source_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("rtpSource");
}

static void *rtp_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return NULL;
}

static void rtp_source_destroy(void *data)
{
	//struct ffmpeg_room *s = data;
}

static void rtp_source_defaults(obs_data_t *settings)
{
}

static obs_properties_t *rtp_source_getproperties(void *data)
{
	//struct ffmpeg_room *s = data;
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	return props;
}

static void rtp_source_activate(void *data)
{
}

static void rtp_source_deactivate(void *data)
{
}

static void rtp_source_tick(void *data, float seconds)
{
}

static void rtp_source_update(void *data, obs_data_t *settings)
{
}

static void get_frame(void *opaque, struct obs_source_frame *f)
{
	//struct ffmpeg_source *s = opaque;
	//obs_source_output_video(s->source, f);
}

static void get_audio(void *opaque, struct obs_source_audio *a)
{
	//struct ffmpeg_source *s = opaque;
	//obs_source_output_audio(s->source, a);
}

void RegisterWinRtpSource()
{
	obs_source_info rtp_source = {};
	rtp_source.id              = "rtp_source";
	rtp_source.type            = OBS_SOURCE_TYPE_INPUT;
	rtp_source.output_flags    = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	rtp_source.get_name        = rtp_source_getname;
	rtp_source.create          = rtp_source_create;
	rtp_source.destroy         = rtp_source_destroy;
	rtp_source.get_defaults    = rtp_source_defaults;
	rtp_source.get_properties  = rtp_source_getproperties;
	rtp_source.activate        = rtp_source_activate;
	rtp_source.deactivate      = rtp_source_deactivate;
	rtp_source.video_tick      = rtp_source_tick;
	rtp_source.update          = rtp_source_update;
}

void RegisterWinRtpOutput()
{
}
