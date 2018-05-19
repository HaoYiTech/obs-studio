/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <assert.h>
#include <inttypes.h>
#include "../util/bmem.h"
#include "../util/platform.h"
#include "../util/profiler.h"
#include "../util/threading.h"
#include "../util/darray.h"

#include "format-conversion.h"
#include "video-io.h"
#include "video-frame.h"
#include "video-scaler.h"

extern profiler_name_store_t *obs_get_profiler_name_store(void);

#define MAX_CONVERT_BUFFERS 3
#define MAX_CACHE_SIZE 16

struct cached_frame_info {
	struct video_data frame;
	int skipped;
	int count;
};

struct video_input {
	struct video_scale_info   conversion;
	video_scaler_t            *scaler;
	struct video_frame        frame[MAX_CONVERT_BUFFERS];
	int                       cur_frame;

	void (*callback)(void *param, struct video_data *frame);
	void *param;
};

static inline void video_input_free(struct video_input *input)
{
	for (size_t i = 0; i < MAX_CONVERT_BUFFERS; i++)
		video_frame_free(&input->frame[i]);
	video_scaler_destroy(input->scaler);
}

struct video_output {
	struct video_output_info   info;

	pthread_t                  thread;
	pthread_mutex_t            data_mutex;
	bool                       stop;

	os_sem_t                   *update_semaphore;
	uint64_t                   frame_time;
	uint32_t                   skipped_frames;
	uint32_t                   total_frames;

	bool                       initialized;

	pthread_mutex_t            input_mutex;
	DARRAY(struct video_input) inputs;

	size_t                     available_frames;
	size_t                     first_added;
	size_t                     last_added;
	struct cached_frame_info   cache[MAX_CACHE_SIZE];
};

/* ------------------------------------------------------------------------- */

static inline bool scale_video_output(struct video_input *input,
		struct video_data *data)
{
	bool success = true;

	if (input->scaler) {
		struct video_frame *frame;

		if (++input->cur_frame == MAX_CONVERT_BUFFERS)
			input->cur_frame = 0;

		frame = &input->frame[input->cur_frame];

		success = video_scaler_scale(input->scaler,
				frame->data, frame->linesize,
				(const uint8_t * const*)data->data,
				data->linesize);

		if (success) {
			for (size_t i = 0; i < MAX_AV_PLANES; i++) {
				data->data[i]     = frame->data[i];
				data->linesize[i] = frame->linesize[i];
			}
		} else {
			blog(LOG_WARNING, "video-io: Could not scale frame!");
		}
	}

	return success;
}

/*
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

static bool FFmpegSaveJpeg(AVCodecContext * pOrigCodecCtx, AVFrame * pOrigFrame, char * lpszJpgName)
{
	AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
	AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
	if (pOutAVCodec == NULL)
		return false;
	bool bReturn = false;
	AVCodecContext * pOutCodecCtx = NULL;
	do {
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// 准备数据结构需要的参数...
		pOutCodecCtx->bit_rate = pOrigCodecCtx->bit_rate;
		pOutCodecCtx->width = pOrigCodecCtx->width;
		pOutCodecCtx->height = pOrigCodecCtx->height;
		// 注意：没有使用适配方式，适配出来格式有可能不是YUVJ420P，造成压缩器崩溃，因为传递的数据已经固定成YUVJ420P了...
		pOutCodecCtx->pix_fmt = avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, -1, 1, 0); //AV_PIX_FMT_YUVJ420P;
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = pOrigCodecCtx->codec_type; //AVMEDIA_TYPE_VIDEO;  
		pOutCodecCtx->time_base.num = 1; //pOrigCodecCtx->time_base.num;  
		pOutCodecCtx->time_base.den = 25; //pOrigCodecCtx->time_base.den;
		// 打开 ffmpeg 压缩器...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// 设置图像质量，默认是0.5，修改为0.8...
		pOutCodecCtx->qcompress = 0.8f;
		// 准备接收缓存，开始压缩jpg数据...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// 采用新的压缩函数...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pOrigFrame, &got_pic);
		// 解码失败或没有得到完整图像，继续解析...
		if (nResult < 0 || !got_pic)
			break;
		// 打开jpg文件句柄...
		FILE * pFile = fopen(lpszJpgName, "wb");
		// 打开jpg失败，注意释放资源...
		if (pFile == NULL) {
			av_packet_unref(&pkt);
			break;
		}
		// 保存到磁盘，并释放资源...
		fwrite(pkt.data, 1, pkt.size, pFile);
		av_packet_unref(&pkt);
		// 释放文件句柄，返回成功...
		fclose(pFile); pFile = NULL;
		bReturn = true;
	} while (false);
	// 清理中间产生的对象...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}
	return bReturn;
}

static bool YUVToJpeg(AVCodecContext * pOrigCodecCtx, AVFrame * pOrigFrame, char * lpszJpgName)
{
	AVFormatContext * pFormatCtx;
	AVOutputFormat * fmt;
	AVCodecContext * pCodecCtx;

	//int nResult = avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, lpszJpgName);
	//fmt = pFormatCtx->oformat;

	pFormatCtx = avformat_alloc_context();
	fmt = av_guess_format("mjpeg", NULL, NULL);
	pFormatCtx->oformat = fmt;
	if (avio_open(&pFormatCtx->pb, lpszJpgName, AVIO_FLAG_READ_WRITE) < 0)
		return false;
	
	AVStream * video_st = avformat_new_stream(pFormatCtx, 0);
	pCodecCtx = video_st->codec;

	AVCodec * pOutAVCodec = avcodec_find_encoder(fmt->video_codec);
	if (pOutAVCodec == NULL)
		return false;

	pCodecCtx = video_st->codec;
	pCodecCtx->codec_id = fmt->video_codec;
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;

	pCodecCtx->width = pOrigCodecCtx->width;
	pCodecCtx->height = pOrigCodecCtx->height;

	pCodecCtx->time_base.num = 1;
	pCodecCtx->time_base.den = 25;

	av_dump_format(pFormatCtx, 0, lpszJpgName, 1);
	
	if (avcodec_open2(pCodecCtx, pOutAVCodec, NULL) < 0)
		return false;
	avformat_write_header(pFormatCtx, NULL);
	AVPacket pkt = { 0 };
	int got_picture = 0;
	int ret = avcodec_encode_video2(pCodecCtx, &pkt, pOrigFrame, &got_picture);
	if (got_picture == 1) {
		pkt.stream_index = video_st->index;
		ret = av_write_frame(pFormatCtx, &pkt);
	}
	av_free_packet(&pkt);
	av_write_trailer(pFormatCtx);
	avcodec_close(video_st->codec);
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);
	return true;
}

void my_av_logoutput(void* ptr, int level, const char* fmt, va_list vl) {
	FILE *fp = fopen("d:/111.log", "a+");
	if (fp) {
		vfprintf(fp, fmt, vl);
		fflush(fp);
		fclose(fp);
	}
}

static inline enum AVPixelFormat obs_to_ffmpeg_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_NONE: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_I444: return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_I420: return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12: return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YVYU: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_YUY2: return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY: return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA: return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800: return AV_PIX_FMT_GRAY8;
	}
	return AV_PIX_FMT_NONE;
}

static bool DoProcSaveJpeg(struct video_output * video, struct video_data * frame, char * lpszJpgName)
{
	/////////////////////////////////////////////////////////////////////////
	// 注意：input->conversion 是需要变换的格式，
	// 因此，应该从 video->info 当中获取原始数据信息...
	// 同时，sws_getContext 需要AVPixelFormat而不是video_format格式...
	/////////////////////////////////////////////////////////////////////////
	// 设置ffmpeg的日志回调函数...
	av_log_set_level(AV_LOG_VERBOSE);
	av_log_set_callback(my_av_logoutput);
	// 统一数据源输入格式，找到压缩器需要的像素格式...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = obs_to_ffmpeg_video_format(video->info.format);
	int nSrcWidth = video->info.width;
	int nSrcHeight = video->info.height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);

	// 注意：这里不用libyuv的原因是，使用sws更简单，不用根据不同像素格式调用不同接口...
	// ffmpeg自带的sws_scale转换也是没有问题的，之前有问题是由于sws_getContext的输入源需要格式AVPixelFormat，写成了video_format，造成的格式错位问题...
	// 注意：目的像素格式不能为AV_PIX_FMT_YUVJ420P，会提示警告信息，但并不影响格式转换，因此，还是使用不会警告的AV_PIX_FMT_YUV420P格式...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);

	// 设置转换后的数据帧内容...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;

	// 将转换后的 YUV 数据存盘成 jpg 文件...
	AVCodecContext * pOutCodecCtx = NULL;
	bool bRetSave = false;
	do {
		// 预先查找jpeg压缩器需要的输入数据格式...
		AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
		AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
		if (pOutAVCodec == NULL)
			break;
		// 创建ffmpeg压缩器的场景对象...
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// 准备数据结构需要的参数...
		pOutCodecCtx->bit_rate = 200000;
		pOutCodecCtx->width = nSrcWidth;
		pOutCodecCtx->height = nSrcHeight;
		// 注意：没有使用适配方式，适配出来格式有可能不是YUVJ420P，造成压缩器崩溃，因为传递的数据已经固定成YUV420P...
		// 注意：输入像素是YUV420P格式，压缩器像素是YUVJ420P格式...
		pOutCodecCtx->pix_fmt = avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, -1, 1, 0); //AV_PIX_FMT_YUVJ420P;
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;  
		pOutCodecCtx->time_base.num = 1;
		pOutCodecCtx->time_base.den = 25;
		 // 打开 ffmpeg 压缩器...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// 设置图像质量，默认是0.5，修改为0.8(图片太大,0.5刚刚好)...
		pOutCodecCtx->qcompress = 0.5f;
		// 准备接收缓存，开始压缩jpg数据...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// 采用新的压缩函数...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pDestFrame, &got_pic);
		// 解码失败或没有得到完整图像，继续解析...
		if (nResult < 0 || !got_pic)
			break;
		// 打开jpg文件句柄...
		FILE * pFile = fopen(lpszJpgName, "wb");
		// 打开jpg失败，注意释放资源...
		if (pFile == NULL) {
			av_packet_unref(&pkt);
			break;
		}
		// 保存到磁盘，并释放资源...
		fwrite(pkt.data, 1, pkt.size, pFile);
		av_packet_unref(&pkt);
		// 释放文件句柄，返回成功...
		fclose(pFile); pFile = NULL;
		bRetSave = true;
	} while (false);
	// 清理中间产生的对象...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}

	// 释放临时分配的数据空间...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);

	return bRetSave;
}*/

static inline bool video_output_cur_frame(struct video_output *video)
{
	struct cached_frame_info *frame_info;
	bool complete;
	bool skipped;

	/* -------------------------------- */

	pthread_mutex_lock(&video->data_mutex);

	frame_info = &video->cache[video->first_added];

	pthread_mutex_unlock(&video->data_mutex);

	/* -------------------------------- */

	pthread_mutex_lock(&video->input_mutex);

	for (size_t i = 0; i < video->inputs.num; i++) {
		struct video_input *input = video->inputs.array+i;
		struct video_data frame = frame_info->frame;

		// 将原始数据存盘成JPEG文件...
		//DoProcSaveJpeg(video, &frame, "d:/111.jpg");

		if (scale_video_output(input, &frame))
			input->callback(input->param, &frame);
	}

	pthread_mutex_unlock(&video->input_mutex);

	/* -------------------------------- */

	pthread_mutex_lock(&video->data_mutex);

	frame_info->frame.timestamp += video->frame_time;
	complete = --frame_info->count == 0;
	skipped = frame_info->skipped > 0;

	if (complete) {
		if (++video->first_added == video->info.cache_size)
			video->first_added = 0;

		if (++video->available_frames == video->info.cache_size)
			video->last_added = video->first_added;
	} else if (skipped) {
		--frame_info->skipped;
		++video->skipped_frames;
	}

	pthread_mutex_unlock(&video->data_mutex);

	/* -------------------------------- */

	return complete;
}

static void *video_thread(void *param)
{
	struct video_output *video = param;

	os_set_thread_name("video-io: video thread");

	const char *video_thread_name =
		profile_store_name(obs_get_profiler_name_store(),
				"video_thread(%s)", video->info.name);

	while (os_sem_wait(video->update_semaphore) == 0) {
		if (video->stop)
			break;

		profile_start(video_thread_name);
		while (!video->stop && !video_output_cur_frame(video)) {
			video->total_frames++;
		}

		video->total_frames++;
		profile_end(video_thread_name);

		profile_reenable_thread();
	}

	return NULL;
}

/* ------------------------------------------------------------------------- */

static inline bool valid_video_params(const struct video_output_info *info)
{
	return info->height != 0 && info->width != 0 && info->fps_den != 0 &&
	       info->fps_num != 0;
}

static inline void init_cache(struct video_output *video)
{
	if (video->info.cache_size > MAX_CACHE_SIZE)
		video->info.cache_size = MAX_CACHE_SIZE;

	for (size_t i = 0; i < video->info.cache_size; i++) {
		struct video_frame *frame;
		frame = (struct video_frame*)&video->cache[i];

		video_frame_init(frame, video->info.format,
				video->info.width, video->info.height);
	}

	video->available_frames = video->info.cache_size;
}

int video_output_open(video_t **video, struct video_output_info *info)
{
	struct video_output *out;
	pthread_mutexattr_t attr;

	if (!valid_video_params(info))
		return VIDEO_OUTPUT_INVALIDPARAM;

	out = bzalloc(sizeof(struct video_output));
	if (!out)
		goto fail;

	memcpy(&out->info, info, sizeof(struct video_output_info));
	out->frame_time = (uint64_t)(1000000000.0 * (double)info->fps_den /
		(double)info->fps_num);
	out->initialized = false;

	if (pthread_mutexattr_init(&attr) != 0)
		goto fail;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail;
	if (pthread_mutex_init(&out->data_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&out->input_mutex, &attr) != 0)
		goto fail;
	if (os_sem_init(&out->update_semaphore, 0) != 0)
		goto fail;
	if (pthread_create(&out->thread, NULL, video_thread, out) != 0)
		goto fail;

	init_cache(out);

	out->initialized = true;
	*video = out;
	return VIDEO_OUTPUT_SUCCESS;

fail:
	video_output_close(out);
	return VIDEO_OUTPUT_FAIL;
}

void video_output_close(video_t *video)
{
	if (!video)
		return;

	video_output_stop(video);

	for (size_t i = 0; i < video->inputs.num; i++)
		video_input_free(&video->inputs.array[i]);
	da_free(video->inputs);

	for (size_t i = 0; i < video->info.cache_size; i++)
		video_frame_free((struct video_frame*)&video->cache[i]);

	os_sem_destroy(video->update_semaphore);
	pthread_mutex_destroy(&video->data_mutex);
	pthread_mutex_destroy(&video->input_mutex);
	bfree(video);
}

static size_t video_get_input_idx(const video_t *video,
		void (*callback)(void *param, struct video_data *frame),
		void *param)
{
	for (size_t i = 0; i < video->inputs.num; i++) {
		struct video_input *input = video->inputs.array+i;
		if (input->callback == callback && input->param == param)
			return i;
	}

	return DARRAY_INVALID;
}

static inline bool video_input_init(struct video_input *input,
		struct video_output *video)
{
	if (input->conversion.width  != video->info.width ||
	    input->conversion.height != video->info.height ||
	    input->conversion.format != video->info.format) {
		struct video_scale_info from = {
			.format = video->info.format,
			.width  = video->info.width,
			.height = video->info.height,
			.range = video->info.range,
			.colorspace = video->info.colorspace
		};

		int ret = video_scaler_create(&input->scaler,
				&input->conversion, &from,
				VIDEO_SCALE_FAST_BILINEAR);
		if (ret != VIDEO_SCALER_SUCCESS) {
			if (ret == VIDEO_SCALER_BAD_CONVERSION)
				blog(LOG_ERROR, "video_input_init: Bad "
				                "scale conversion type");
			else
				blog(LOG_ERROR, "video_input_init: Failed to "
				                "create scaler");

			return false;
		}

		for (size_t i = 0; i < MAX_CONVERT_BUFFERS; i++)
			video_frame_init(&input->frame[i],
					input->conversion.format,
					input->conversion.width,
					input->conversion.height);
	}

	return true;
}

bool video_output_connect(video_t *video,
		const struct video_scale_info *conversion,
		void (*callback)(void *param, struct video_data *frame),
		void *param)
{
	bool success = false;

	if (!video || !callback)
		return false;

	pthread_mutex_lock(&video->input_mutex);

	if (video->inputs.num == 0) {
		video->skipped_frames = 0;
		video->total_frames = 0;
	}

	if (video_get_input_idx(video, callback, param) == DARRAY_INVALID) {
		struct video_input input;
		memset(&input, 0, sizeof(input));

		input.callback = callback;
		input.param    = param;

		if (conversion) {
			input.conversion = *conversion;
		} else {
			input.conversion.format    = video->info.format;
			input.conversion.width     = video->info.width;
			input.conversion.height    = video->info.height;
		}

		if (input.conversion.width == 0)
			input.conversion.width = video->info.width;
		if (input.conversion.height == 0)
			input.conversion.height = video->info.height;

		success = video_input_init(&input, video);
		if (success)
			da_push_back(video->inputs, &input);
	}

	pthread_mutex_unlock(&video->input_mutex);

	return success;
}

void video_output_disconnect(video_t *video,
		void (*callback)(void *param, struct video_data *frame),
		void *param)
{
	if (!video || !callback)
		return;

	pthread_mutex_lock(&video->input_mutex);

	size_t idx = video_get_input_idx(video, callback, param);
	if (idx != DARRAY_INVALID) {
		video_input_free(video->inputs.array+idx);
		da_erase(video->inputs, idx);
	}

	if (video->inputs.num == 0) {
		double percentage_skipped = (double)video->skipped_frames /
			(double)video->total_frames * 100.0;

		if (video->skipped_frames)
			blog(LOG_INFO, "Video stopped, number of "
					"skipped frames due "
					"to encoding lag: "
					"%"PRIu32"/%"PRIu32" (%0.1f%%)",
					video->skipped_frames,
					video->total_frames,
					percentage_skipped);
	}

	pthread_mutex_unlock(&video->input_mutex);
}

bool video_output_active(const video_t *video)
{
	if (!video) return false;
	return video->inputs.num != 0;
}

const struct video_output_info *video_output_get_info(const video_t *video)
{
	return video ? &video->info : NULL;
}

bool video_output_lock_frame(video_t *video, struct video_frame *frame,
		int count, uint64_t timestamp)
{
	struct cached_frame_info *cfi;
	bool locked;

	if (!video) return false;

	pthread_mutex_lock(&video->data_mutex);

	if (video->available_frames == 0) {
		video->cache[video->last_added].count += count;
		video->cache[video->last_added].skipped += count;
		locked = false;

	} else {
		if (video->available_frames != video->info.cache_size) {
			if (++video->last_added == video->info.cache_size)
				video->last_added = 0;
		}

		cfi = &video->cache[video->last_added];
		cfi->frame.timestamp = timestamp;
		cfi->count = count;
		cfi->skipped = 0;

		memcpy(frame, &cfi->frame, sizeof(*frame));

		locked = true;
	}

	pthread_mutex_unlock(&video->data_mutex);

	return locked;
}

void video_output_unlock_frame(video_t *video)
{
	if (!video) return;

	pthread_mutex_lock(&video->data_mutex);

	video->available_frames--;
	os_sem_post(video->update_semaphore);

	pthread_mutex_unlock(&video->data_mutex);
}

uint64_t video_output_get_frame_time(const video_t *video)
{
	return video ? video->frame_time : 0;
}

void video_output_stop(video_t *video)
{
	void *thread_ret;

	if (!video)
		return;

	if (video->initialized) {
		video->initialized = false;
		video->stop = true;
		os_sem_post(video->update_semaphore);
		pthread_join(video->thread, &thread_ret);
	}
}

bool video_output_stopped(video_t *video)
{
	if (!video)
		return true;

	return video->stop;
}

enum video_format video_output_get_format(const video_t *video)
{
	return video ? video->info.format : VIDEO_FORMAT_NONE;
}

uint32_t video_output_get_width(const video_t *video)
{
	return video ? video->info.width : 0;
}

uint32_t video_output_get_height(const video_t *video)
{
	return video ? video->info.height : 0;
}

double video_output_get_frame_rate(const video_t *video)
{
	if (!video)
		return 0.0;

	return (double)video->info.fps_num / (double)video->info.fps_den;
}

uint32_t video_output_get_skipped_frames(const video_t *video)
{
	return video->skipped_frames;
}

uint32_t video_output_get_total_frames(const video_t *video)
{
	return video->total_frames;
}
