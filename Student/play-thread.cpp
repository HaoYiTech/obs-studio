
#include "BitWritter.h"
#include "student-app.h"
#include "play-thread.h"
#include "window-view-render.hpp"
#include "window-view-camera.hpp"

CVideoPlay::CVideoPlay(CViewRender * lpViewPlayer, int64_t inSysZeroNS)
  : m_lpViewPlayer(lpViewPlayer)
  , m_sys_zero_ns(inSysZeroNS)
  , m_nSDLTextureHeight(0)
  , m_nSDLTextureWidth(0)
  , m_img_buffer_ptr(NULL)
  , m_img_buffer_size(0)
  , m_lpDecContext(NULL)
  , m_lpDecFrame(NULL)
  , m_lpDecCodec(NULL)
  , m_bNeedSleep(false)
  , m_play_next_ns(-1)
  , m_sdlRenderer(NULL)
  , m_sdlTexture(NULL)
  , m_sdlScreen(NULL)
{
	//m_bUdpRecvThreadStop = false;
	// 初始化解码器互斥对象...
	pthread_mutex_init_value(&m_VideoMutex);
	// 初始化COM系统对象...
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	// 初始化SDL2.0 => QT线程当中已经调用了 CoInitializeEx()...
	int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
}

CVideoPlay::~CVideoPlay()
{
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 关闭音频解码对象...
	this->doDecoderFree();
	// 销毁SDL窗口...
	if (m_sdlScreen != NULL) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	if (m_sdlRenderer != NULL) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	if (m_sdlTexture != NULL) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// 销毁SDL窗口时会隐藏关联窗口...
	if (m_lpViewPlayer != NULL) {
		HWND hWnd = m_lpViewPlayer->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
	}
	// 释放单帧图像转换空间...
	if (m_img_buffer_ptr != NULL) {
		av_free(m_img_buffer_ptr);
		m_img_buffer_ptr = NULL;
		m_img_buffer_size = 0;
	}
	// 释放解码器互斥对象...
	pthread_mutex_destroy(&m_VideoMutex);
}

// 判断SDL是否需要重建...
bool CVideoPlay::IsCanRebuildSDL(int nPicWidth, int nPicHeight)
{
	// 如果右侧播放线程停止引发的状态变化，SDL的退出会导致绘制重建...
	/*if (m_bUdpRecvThreadStop) {
		// 注意：右侧SDL的销毁，必须重新初始胡SDL才能成功创建，否则不能重建SDL...
		int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
		m_lpViewPlayer->GetAndResetRenderFlag();
		m_bUdpRecvThreadStop = false;
		return true;
	}*/
	// 纹理的宽高发生变化，需要重建，需要清理窗口变化，避免重复创建...
	if (m_nSDLTextureWidth != nPicWidth || m_nSDLTextureHeight != nPicHeight) {
		m_lpViewPlayer->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_nSDLTextureWidth == nPicWidth && m_nSDLTextureHeight == nPicHeight);
	// SDL相关对象无效，需要重建，需要清理窗口变化，避免重复创建...
	if (m_sdlScreen == NULL || m_sdlRenderer == NULL || m_sdlTexture == NULL) {
		m_lpViewPlayer->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_sdlScreen != NULL && m_sdlRenderer != NULL && m_sdlTexture != NULL);
	return m_lpViewPlayer->GetAndResetRenderFlag();
}

void CVideoPlay::doReBuildSDL(int nPicWidth, int nPicHeight)
{
	// 销毁SDL窗口 => 会自动隐藏关联窗口...
	if (m_sdlScreen != NULL) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	// 销毁渲染器...
	if (m_sdlRenderer != NULL) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	// 销毁纹理...
	if (m_sdlTexture != NULL) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// 重建SDL相关对象...
	if (m_lpViewPlayer != NULL) {
		// 销毁SDL窗口时会隐藏关联窗口 => 必须用Windows的API接口...
		HWND hWnd = m_lpViewPlayer->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
		// 创建SDL需要的对象 => 窗口、渲染、纹理...
		m_sdlScreen = SDL_CreateWindowFrom((void*)hWnd);
		m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
		m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, nPicWidth, nPicHeight);
		// 保存Texture的高宽，当发生高宽改变时，做为重建Texture的凭证依据...
		m_nSDLTextureHeight = nPicHeight;
		m_nSDLTextureWidth = nPicWidth;
	}
}

void CVideoPlay::doDecoderFree()
{
	// 释放解码结构体...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// 释放解码器对象...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
	}
	// 释放队列中的解码前的数据块...
	GM_AVPacket::iterator itorPack;
	for (itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// 释放没有播放完毕的解码后的数据帧...
	GM_AVFrame::iterator itorFrame;
	for (itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
}

bool CVideoPlay::doInitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	///////////////////////////////////////////////////////////////////////
	// 注意：这里丢弃了nWidth|nHeight|nFPS，使用解码之后的图片高宽更精确...
	///////////////////////////////////////////////////////////////////////
	// 如果已经初始化，直接返回...
	if (m_lpDecCodec != NULL || m_lpDecContext != NULL)
		return false;
	ASSERT(m_lpDecCodec == NULL && m_lpDecContext == NULL);
	// 保存传递过来的参数信息...
	m_strSPS = inSPS;
	m_strPPS = inPPS;
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// 查找需要的解码器 => 不用创建解析器...
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// 设置支持低延时模式 => 没啥作用，必须配合具体过程...
	m_lpDecContext->flags |= CODEC_FLAG_LOW_DELAY;
	// 设置支持不完整片段解码 => 没啥作用...
	if (m_lpDecCodec->capabilities & CODEC_CAP_TRUNCATED) {
		m_lpDecContext->flags |= CODEC_FLAG_TRUNCATED;
	}
	// 设置解码器关联参数配置 => 这里设置不起作用...
	//m_lpDecContext->refcounted_frames = 1;
	//m_lpDecContext->has_b_frames = 0;
	// 打开获取到的解码器...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) < 0) {
		blog(LOG_INFO, "%s [Video] avcodec_open2 failed.", TM_RECV_NAME);
		return false;
	}
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);
	// 启动线程开始运转...
	this->Start();
	return true;
}

void CVideoPlay::doPushFrame(FMS_FRAME & inFrame, int inCalcPTS)
{
	// 线程正在退出中，直接返回...
	if (this->IsStopRequested())
		return;
	// 临时保存传递过来的数据帧参数信息...
	string & inData = inFrame.strData;
	bool bIsKeyFrame = inFrame.is_keyframe;
	int inOffset = inFrame.dwRenderOffset;
	int inPTS = inCalcPTS;
	// 每个关键帧都放入sps和pps，播放会加快...
	string strCurFrame;
	// 如果是关键帧，必须先写入sps，再写如pps...
	if (bIsKeyFrame) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// 修改视频帧的起始码 => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// 最后追加新数据...
	strCurFrame.append(inData);
	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// 计算当前帧的PTS，关键帧标志 => 视频流 => 1
	// 目前只有I帧和P帧，PTS和DTS是一致的...
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// 将数据压入解码前队列当中 => 需要与解码线程互斥...
	pthread_mutex_lock(&m_VideoMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_VideoMutex);
}

void CVideoPlay::doSleepTo()
{
	// < 0 不能休息，有不能休息标志 => 都直接返回...
	if (!m_bNeedSleep || m_play_next_ns < 0)
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// 如果比当前时间小(已过期)，直接返回...
	if (m_play_next_ns <= cur_time_ns)
		return;
	// 计算超前时间的差值，最多休息10毫秒...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// 调用系统工具函数，进行sleep休息...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

void CVideoPlay::doDecodeFrame()
{
	// 没有要解码的数据包，直接返回最大休息毫秒数...
	if (m_MapPacket.size() <= 0) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 抽取一个AVPacket进行解码操作，理论上一个AVPacket一定能解码出一个Picture...
	// 由于数据丢失或B帧，投递一个AVPacket不一定能返回Picture，这时解码器就会将数据缓存起来，造成解码延时...
	int got_picture = 0, nResult = 0;
	// 为了快速解码，只要有数据就解码，同时与数据输入线程互斥...
	pthread_mutex_lock(&m_VideoMutex);
	GM_AVPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_VideoMutex);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：由于只有 I帧 和 P帧，没有B帧，要让解码器快速丢掉解码错误数据，通过设置has_b_frames来完成
	// 技术文档 => https://bbs.csdn.net/topics/390692774
	//////////////////////////////////////////////////////////////////////////////////////////////////
	m_lpDecContext->has_b_frames = 0;
	// 将完整压缩数据帧放入解码器进行解码操作...
	nResult = avcodec_decode_video2(m_lpDecContext, m_lpDecFrame, &got_picture, &thePacket);
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：目前只有 I帧 和 P帧  => 这里使用全局AVFrame，非常重要，能提供后续AVPacket的解码支持...
	// 解码失败或没有得到完整图像 => 是需要后续AVPacket的数据才能解码出图像...
	// 非常关键的操作 => m_lpDFrame 千万不能释放，继续灌AVPacket就能解码出图像...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if (nResult < 0 || !got_picture) {
		// 打印解码失败信息，显示坏帧的个数...
		//blog(LOG_INFO, "%s [Video] %s Error => decode_frame failed, BFrame: %d, PTS: %I64d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, (m_lpRenderWnd != NULL) ? "net" : "loc", m_lpDecoder->has_b_frames, thePacket.pts + inStartPtsMS, nResult, thePacket.size);
		// 这里非常关键，告诉解码器不要缓存坏帧(B帧)，一旦有解码错误，直接扔掉，这就是低延时解码模式...
		m_lpDecContext->has_b_frames = 0;
		// 丢掉解码失败的数据帧...
		av_free_packet(&thePacket);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里使用AVFrame计算的最佳有效时间戳，使用AVPacket的时间戳也是一样的...
	// 因为，采用了低延时的模式，坏帧都扔掉了，解码器内部没有缓存数据，不会出现时间戳错位问题...
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = m_lpDecFrame->best_effort_timestamp;
	// 重新克隆AVFrame，自动分配空间，按时间排序...
	AVFrame * lpNewFrame = av_frame_clone(m_lpDecFrame);
	m_MapFrame.insert(pair<int64_t, AVFrame*>(frame_pts_ms, lpNewFrame));
	//DoProcSaveJpeg(m_lpDFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// 这里需要释放AVPacket的缓存...
	av_free_packet(&thePacket);
	// 修改休息状态 => 已经有解码，不能休息...
	m_bNeedSleep = false;
}

void CVideoPlay::doDisplayVideo()
{
	/////////////////////////////////////////////////////////////////
	// 注意：这里产生的AVFrame是同一个线程顺序执行，无需互斥...
	/////////////////////////////////////////////////////////////////
	// 如果没有已解码数据帧，直接返回休息最大毫秒数...
	if (m_MapFrame.size() <= 0) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 计算当前时刻点与系统0点位置的时间差 => 转换成毫秒...
	int64_t sys_cur_ms = (os_gettime_ns() - m_sys_zero_ns) / 1000000;
	// 取出第一个已解码数据帧 => 时间最小的数据帧...
	GM_AVFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame = itorItem->second;
	int64_t   frame_pts_ms = itorItem->first;
	// 当前帧的显示时间还没有到 => 直接休息差值...
	if (frame_pts_ms > sys_cur_ms) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms) * 1000000;
		//blog(LOG_INFO, "%s [Video] Advance => PTS: %I64d, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, frame_pts_ms - sys_cur_ms, m_MapPacket.size(), m_MapFrame.size());
		return;
	}
	// 打印正在播放的解码后视频数据...
	//blog(LOG_INFO, "%s [Video] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), m_MapFrame.size());
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：视频延时帧（落后帧），不能丢弃，必须继续显示，视频消耗速度相对较快，除非时间戳给错了，会造成播放问题。
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 准备需要转换的格式信息...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecContext->pix_fmt;
	// 解码后的高宽，可能与预设的高宽不一致...
	int nSrcWidth = m_lpDecContext->width;
	int nSrcHeight = m_lpDecContext->height;
	int nDstWidth = m_lpDecContext->width;   // 没有使用预设值m_nDstWidth，海康的rtsp获取高宽有问题;
	int nDstHeight = m_lpDecContext->height; // 没有使用预设值m_nDstHeight，海康的rtsp获取高宽有问题;
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：解码后的高宽，可能与预设高宽不一致...
	// 注意：不管什么格式，都需要进行像素格式的转换...
	// 注意：必须按照解码后的图像高宽进行转换，预先分配转换空间，避免来回创建释放临时空间...
	// 注意：以前使用预设值是由于要先创建SDL纹理，现在改进为可以后创建SDL纹理，预设的高宽就没有用了，只需要解码后的实际高宽就可以了...
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 计算格式转换需要的缓存空间，并对缓存空间进行重新分配...
	int dst_buffer_size = avpicture_get_size(nDestFormat, nDstWidth, nDstHeight);
	if (dst_buffer_size > m_img_buffer_size) {
		// 删除之前分配的单帧最大空间...
		if (m_img_buffer_ptr != NULL) {
			av_free(m_img_buffer_ptr);
			m_img_buffer_ptr = NULL;
		}
		// 重新分配最新的单帧最大空间...
		m_img_buffer_size = dst_buffer_size;
		m_img_buffer_ptr = (uint8_t *)av_malloc(m_img_buffer_size);
	}
	// 构造新的目标帧对象...
	AVPicture pDestFrame = { 0 };
	avpicture_fill(&pDestFrame, m_img_buffer_ptr, nDestFormat, nDstWidth, nDstHeight);
	// 调用ffmpeg的格式转换接口函数，这里只进行图像格式的转换，不对图像的大小进行缩放...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nDstWidth, nDstHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame.data, pDestFrame.linesize);
	sws_freeContext(img_convert_ctx);
	//////////////////////////////////////////////////////////////////////////////////
	// 使用SDL 进行画面绘制工作 => 正在处理全屏时，不能绘制，会与D3D发生冲突崩溃...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_lpViewPlayer != NULL && !m_lpViewPlayer->IsChangeScreen()) {
		// 用解码后图像高宽去判断是否需要重建SDL，播放窗口发生变化也要重建...
		if (this->IsCanRebuildSDL(nSrcWidth, nSrcHeight)) {
			this->doReBuildSDL(nSrcWidth, nSrcHeight);
		}
		// 获取渲染窗口的矩形区域...
		QRect & rcRect = m_lpViewPlayer->GetRenderRect();
		// 注意：这里的源是转换后的图像，目的是播放窗口..
		SDL_Rect srcSdlRect = { 0 };
		SDL_Rect dstSdlRect = { 0 };
		srcSdlRect.w = nDstWidth;
		srcSdlRect.h = nDstHeight;
		dstSdlRect.x = rcRect.left();
		dstSdlRect.y = rcRect.top();
		dstSdlRect.w = rcRect.width();
		dstSdlRect.h = rcRect.height();
		// 计算源头和目的宽高比例，做为变换基线参考...
		float srcRatio = srcSdlRect.w * 1.0f / srcSdlRect.h;
		float dstRatio = dstSdlRect.w * 1.0f / dstSdlRect.h;
		// 先把画面绘制的Texture上，再把Texture缩放到播放窗口上面，Texture的大小在创建时使用的是预设高宽...
		int nResult = SDL_UpdateTexture(m_sdlTexture, &srcSdlRect, pDestFrame.data[0], pDestFrame.linesize[0]);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		nResult = SDL_RenderClear(m_sdlRenderer);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// 如果源头比例比目的比例大，用宽度做为基线，否则用高度做为基线...
		if (srcRatio >= dstRatio) {
			// 计算目的窗口矩形的高度值(宽不变) => 源(高)/源(宽) = 目(高)/目(宽)
			dstSdlRect.h = srcSdlRect.h * dstSdlRect.w / srcSdlRect.w;
			dstSdlRect.y = (rcRect.height() - dstSdlRect.h) / 2;
		} else {
			// 计算目的窗口矩形的宽度值(高不变) => 源(高)/源(宽) = 目(高)/目(宽)
			dstSdlRect.w = srcSdlRect.w * dstSdlRect.h / srcSdlRect.h;
			dstSdlRect.x = (rcRect.width() - dstSdlRect.w) / 2;
		}
		// 将计算后的矩形区域进行拷贝显示到渲染窗口当中...
		nResult = SDL_RenderCopy(m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// 正式激发渲染窗口，绘制出图像来...
		SDL_RenderPresent(m_sdlRenderer);
	}
	// 释放并删除已经使用完毕原始数据块...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
	// 修改休息状态 => 已经有播放，不能休息...
	m_bNeedSleep = false;
}

void CVideoPlay::doPushPacket(AVPacket & inPacket)
{
	// 注意：这里必须以DTS排序，决定了解码的先后顺序...
	// 注意：由于使用multimap，可以专门处理相同时间戳...
	m_MapPacket.insert(pair<int64_t, AVPacket>(inPacket.dts, inPacket));
}

void CVideoPlay::Entry()
{
	while (!this->IsStopRequested()) {
		// 设置休息标志 => 只要有解码或播放就不能休息...
		m_bNeedSleep = true;
		// 解码一帧音频...
		this->doDecodeFrame();
		// 显示一帧音频...
		this->doDisplayVideo();
		// 进行sleep等待...
		this->doSleepTo();
	}
}

CAudioPlay::CAudioPlay(CViewCamera * lpViewCamera, int64_t inSysZeroNS)
  : m_lpViewCamera(lpViewCamera)
  , m_sys_zero_ns(inSysZeroNS)
  , m_horn_resampler(NULL)
  , m_lpDecContext(NULL)
  , m_lpDecFrame(NULL)
  , m_lpDecCodec(NULL)
  , m_bNeedSleep(false)
  , m_play_next_ns(-1)
  , m_fVolRate(1.0f)
  , m_bIsMute(false)
  , m_frame_num(0)
  , m_device(NULL)
  , m_client(NULL)
  , m_render(NULL)
{
	m_out_frame_bytes = 0;
	m_out_frame_duration = 0;
	// 设置默认参数值...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	m_in_sample_fmt = AV_SAMPLE_FMT_FLTP;
	// 设置默认输出声道、输出采样率...
	m_out_channel_num = 0;
	m_out_sample_rate = 0;
	m_out_sample_fmt = AV_SAMPLE_FMT_FLT;
	// 初始化PCM数据环形队列...
	circlebuf_init(&m_circle);
	// 初始化解码器互斥对象...
	pthread_mutex_init_value(&m_AudioMutex);
	// 初始化COM系统对象...
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

CAudioPlay::~CAudioPlay()
{
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 关闭音频解码对象...
	this->doDecoderFree();
	// 关闭音频监听设备...
	this->doMonitorFree();
	// 释放音频环形队列...
	circlebuf_free(&m_circle);
	// 释放解码器互斥对象...
	pthread_mutex_destroy(&m_AudioMutex);
}

void CAudioPlay::doDecoderFree()
{
	// 释放解码结构体...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// 释放解码器对象...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
	}
	// 释放队列中的解码前的数据块...
	GM_AVPacket::iterator itorPack;
	for (itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// 释放扬声器音频转换器...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
}

void CAudioPlay::doMonitorFree()
{
	// 停止WSAPI音频资源...
	ULONG uRef = 0;
	HRESULT hr = S_OK;
	if (m_client != NULL) {
		hr = m_client->Stop();
	}
	// 释放WSAPI引用对象...
	if (m_device != NULL) {
		uRef = m_device->Release();
	}
	if (m_client != NULL) {
		uRef = m_client->Release();
	}
	if (m_render != NULL) {
		uRef = m_render->Release();
	}
}

bool CAudioPlay::doInitAudio(int nInRateIndex, int nInChannelNum)
{
	// 如果已经初始化，直接返回...
	if (m_lpDecCodec != NULL || m_lpDecContext != NULL)
		return false;
	ASSERT(m_lpDecCodec == NULL && m_lpDecContext == NULL);
	// 开始对WSAPI的音频进行初始化工作...
	IMMDeviceEnumerator *immde = NULL;
	WAVEFORMATEX *wfex = NULL;
	bool success = false;
	HRESULT hr = S_OK;
	UINT32 frames = 0;
	do {
		// 创建设备枚举对象...
		hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&immde);
		if (FAILED(hr))
			break;
		hr = immde->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
		if (FAILED(hr))
			break;
		hr = m_device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&m_client);
		if (FAILED(hr))
			break;
		hr = m_client->GetMixFormat(&wfex);
		if (FAILED(hr))
			break;
		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, wfex, NULL);
		if (FAILED(hr))
			break;
		// 保存WASAPI输出的采样率和声道数...
		m_out_sample_rate = wfex->nSamplesPerSec;
		m_out_channel_num = wfex->nChannels;
		m_out_sample_fmt = AV_SAMPLE_FMT_FLT;
		hr = m_client->GetBufferSize(&frames);
		if (FAILED(hr))
			break;
		hr = m_client->GetService(IID_IAudioRenderClient, (void**)&m_render);
		if (FAILED(hr))
			break;
		hr = m_client->Start();
		if (FAILED(hr))
			break;
		success = true;
	} while (false);
	// 释放不需要的中间变量...
	if (immde != NULL) {
		immde->Release();
	}
	if (wfex != NULL) {
		CoTaskMemFree(wfex);
	}
	// 如果初始化失败，直接返回...
	if (!success) {
		this->doMonitorFree();
		return false;
	}
	// 根据索引获取采样率...
	switch (nInRateIndex)
	{
	case 0x03: m_in_sample_rate = 48000; break;
	case 0x04: m_in_sample_rate = 44100; break;
	case 0x05: m_in_sample_rate = 32000; break;
	case 0x06: m_in_sample_rate = 24000; break;
	case 0x07: m_in_sample_rate = 22050; break;
	case 0x08: m_in_sample_rate = 16000; break;
	case 0x09: m_in_sample_rate = 12000; break;
	case 0x0a: m_in_sample_rate = 11025; break;
	case 0x0b: m_in_sample_rate = 8000;  break;
	default:   m_in_sample_rate = 48000; break;
	}
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// 打开获取到的解码器...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) != 0) {
		blog(LOG_INFO, "Error => CAudioPlay::doInitAudio() => avcodec_open2");
		return false;
	}
	// 保存输入采样率索引和声道...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	m_in_sample_fmt = m_lpDecContext->sample_fmt;
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);
	// 设置解码后的音频格式...
	resample_info decInfo = { 0 };
	decInfo.samples_per_sec = m_in_sample_rate;
	decInfo.speakers = CStudentApp::convert_speaker_layout(m_in_channel_num);
	decInfo.format = CStudentApp::convert_sample_format(m_in_sample_fmt);
	// 设置音频输出到扬声器(WASAPI)需要的格式...
	m_horn_sample_info.samples_per_sec = m_out_sample_rate;
	m_horn_sample_info.speakers = CStudentApp::convert_speaker_layout(m_out_channel_num);
	m_horn_sample_info.format = CStudentApp::convert_sample_format(m_out_sample_fmt);
	// 创建扬声器回放的重采样对象，解码后的音频数据要进行一次适配扬声器的重采样...
	m_horn_resampler = audio_resampler_create(&m_horn_sample_info, &decInfo);
	// 创建重采样对象失败，打印错误返回...
	if (m_horn_resampler == NULL) {
		blog(LOG_INFO, "Error => CAudioPlay::doInitAudio() => audio_resampler_create");
		return false;
	}
	// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	int in_nb_samples = 1024;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, m_out_sample_rate, m_in_sample_rate, AV_ROUND_UP);
	// 计算输出每帧持续时间(毫秒)，每帧字节数 => 只需要计算一次就够了...
	m_out_frame_duration = out_nb_samples * 1000 / m_out_sample_rate;
	m_out_frame_bytes = av_samples_get_buffer_size(NULL, m_out_channel_num, out_nb_samples, m_out_sample_fmt, 1);
	// 启动线程...
	this->Start();
	return true;
}

// 音频数据帧的输入接口，为了不阻塞主线程，直接投递到缓存当中...
void CAudioPlay::doPushFrame(FMS_FRAME & inFrame, int inCalcPTS)
{
	// 线程正在退出中，直接返回...
	if (this->IsStopRequested())
		return;
	// 不是音频数据帧，直接返回...
	if (inFrame.typeFlvTag != PT_TAG_AUDIO)
		return;
	// 如果解码对象为空，直接返回失败...
	if (m_lpDecContext == NULL || m_lpDecCodec == NULL || m_lpDecFrame == NULL)
		return;
	// 如果扬声器音频转换器无效，直接返回...
	if (m_horn_resampler == NULL)
		return;
	// 加上ADTS数据包头...
	string & inData = inFrame.strData;
	uint32_t inPTS = inCalcPTS; //inFrame.dwSendTime;
	uint32_t inOffset = inFrame.dwRenderOffset;
	bool bIsKeyFrame = inFrame.is_keyframe;
	// 先加入ADTS头，再加入数据帧内容...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// 构造ADTS帧头...
	PutBitContext pb;
	char pbuf[ADTS_HEADER_SIZE * 2] = { 0 };
	init_put_bits(&pb, pbuf, ADTS_HEADER_SIZE);
	/* adts_fixed_header */
	put_bits(&pb, 12, 0xfff);   /* syncword */
	put_bits(&pb, 1, 0);        /* ID */
	put_bits(&pb, 2, 0);        /* layer */
	put_bits(&pb, 1, 1);        /* protection_absent */
	put_bits(&pb, 2, 2);		/* profile_objecttype */
	put_bits(&pb, 4, m_in_rate_index);
	put_bits(&pb, 1, 0);        /* private_bit */
	put_bits(&pb, 3, m_in_channel_num); /* channel_configuration */
	put_bits(&pb, 1, 0);        /* original_copy */
	put_bits(&pb, 1, 0);        /* home */
								/* adts_variable_header */
	put_bits(&pb, 1, 0);        /* copyright_identification_bit */
	put_bits(&pb, 1, 0);        /* copyright_identification_start */
	put_bits(&pb, 13, nTotalSize); /* aac_frame_length */
	put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
	put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

	flush_put_bits(&pb);

	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// 先添加帧头数据，再添加帧内容信息...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// 计算当前帧的PTS，关键帧标志 => 音频流 => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	// 将数据压入解码前队列当中 => 需要与解码线程互斥...
	pthread_mutex_lock(&m_AudioMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_AudioMutex);
}

void CAudioPlay::doPushPacket(AVPacket & inPacket)
{
	// 注意：这里必须以DTS排序，决定了解码的先后顺序...
	// 注意：由于使用multimap，可以专门处理相同时间戳...
	m_MapPacket.insert(pair<int64_t, AVPacket>(inPacket.dts, inPacket));
}

void CAudioPlay::doDecodeFrame()
{
	// 没有要解码的数据包，直接返回最大休息毫秒数...
	if (m_MapPacket.size() <= 0 || m_render == NULL) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 抽取一个AVPacket进行解码操作，一个AVPacket不一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	// 为了快速解码，只要有数据就解码，同时与数据输入线程互斥...
	pthread_mutex_lock(&m_AudioMutex);
	GM_AVPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_AudioMutex);
	// 注意：这里解码后的格式是AV_SAMPLE_FMT_FLTP，需要转换成扬声器需要float格式...
	nResult = avcodec_decode_audio4(m_lpDecContext, m_lpDecFrame, &got_picture, &thePacket);
	// 如果没有解出完整数据包，打印信息，释放数据包...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", thePacket.pts, nResult, thePacket.size);
		av_free_packet(&thePacket);
		return;
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	int64_t   frame_pts_ms = thePacket.pts;
	// 对原始解码后的音频样本格式，转换成扬声器需要的样本格式，转换成功放入环形队列...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, m_lpDecFrame->data, m_lpDecFrame->nb_samples)) {
		// 计算格式转换之后的数据内容长度，并将转换后的数据放入组块缓存当中...
		int cur_data_size = get_audio_size(m_horn_sample_info.format, m_horn_sample_info.speakers, output_frames);
		// 压入环形队列当中，不用开启互斥对象 => PTS|Size|Data
		circlebuf_push_back(&m_circle, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle, &cur_data_size, sizeof(int));
		circlebuf_push_back(&m_circle, output_data[0], cur_data_size);
		//blog(LOG_INFO, "%s [Audio-Decode] CurTime: %I64d ms, PTS: %I64d ms, Frame: %d, Duration:%.2f", TM_RECV_NAME,
		//	os_gettime_ns()/1000000, frame_pts_ms, output_frames, 1000*output_frames/(1.0f*m_horn_sample_info.samples_per_sec));
		// 累加环形队列中有效数据帧的个数...
		++m_frame_num;
	}
	// 释放AVPacket数据包...
	av_free_packet(&thePacket);
	// 不能休息，需要继续解码...
	m_bNeedSleep = false;
}

void CAudioPlay::doDisplayAudio()
{
	// 如果没有已解码数据帧，直接返回最大休息毫秒数...
	if (m_circle.size <= 0 || m_render == NULL) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 计算当前时间与0点位置的时间差 => 转换成毫秒...
	int64_t sys_cur_ms = (os_gettime_ns() - m_sys_zero_ns) / 1000000;
	// 获取第一个已解码数据帧 => 时间最小的数据帧...
	int64_t frame_pts_ms = 0; int out_buffer_size = 0;
	circlebuf_peek_front(&m_circle, &frame_pts_ms, sizeof(int64_t));
	// 不能超前投递数据，会造成硬件层数据堆积，造成缓存积压，引发缓存清理...
	if (frame_pts_ms > sys_cur_ms) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms) * 1000000;
		return;
	}
	// 当前帧时间戳从环形队列当中移除 => 后面是数据帧长度...
	circlebuf_pop_front(&m_circle, NULL, sizeof(int64_t));
	// 从环形队列当中，取出音频数据帧内容，动态长度...
	circlebuf_pop_front(&m_circle, &out_buffer_size, sizeof(int));
	// 如果是静音状态，直接丢弃音频数据，直接返回...
	if (m_bIsMute) {
		// 删除已经使用的音频数据 => 从环形队列中移除...
		circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
		// 减少环形队列中有效数据帧的个数...
		--m_frame_num;
		// 已经有播放，不能休息...
		m_bNeedSleep = false;
		return;
	}
	// 根据数据库的长度为PCM分配数据空间...
	if (out_buffer_size > m_strHorn.size()) {
		m_strHorn.resize(out_buffer_size);
	}
	// 当前数据帧长度一定小于或等于最大单帧输出空间...
	ASSERT(out_buffer_size <= m_strHorn.size());
	// 从环形队列读取当前帧内容，因为是顺序执行，可以再次使用单帧最大输出空间...
	circlebuf_peek_front(&m_circle, (void*)m_strHorn.c_str(), out_buffer_size);

	// 计算当前数据块包含的有效采样个数...
	BYTE * out_buffer_ptr = (BYTE*)m_strHorn.c_str();
	int nPerFrameSize = (m_out_channel_num * sizeof(float));
	uint32_t resample_frames = out_buffer_size / nPerFrameSize;

	// 设置音量数据的转换 => 这里进行音量的放大...
	float vol = m_fVolRate;
	if (!close_float(vol, 1.0f, EPSILON)) {
		register float *cur = (float*)out_buffer_ptr;
		register float *end = cur + resample_frames * m_out_channel_num;
		while (cur < end) {
			*(cur++) *= vol;
		}
	}

	HRESULT hr = S_OK;
	BYTE * output = NULL;
	UINT32 nCurPadFrame = 0;
	UINT32 nAllowQueueFrame = 0;
	int    msInSndCardBuf = 0;
	// 获取当前声卡已缓存的帧数量...
	hr = m_client->GetCurrentPadding(&nCurPadFrame);
	// 计算在声卡中已缓存的毫秒数 => 向下取整...
	msInSndCardBuf = (int)((nCurPadFrame * 1.0f * nPerFrameSize / m_out_frame_bytes) * m_out_frame_duration + 0.5f);
	// 将300毫秒的声卡缓存，转换成帧数量...
	nAllowQueueFrame = (300.0f / m_out_frame_duration * m_out_frame_bytes) / nPerFrameSize;
	// 只有当声卡缓存小于300毫秒时，才进行数据投递，大于300毫秒，直接丢弃...
	if (nCurPadFrame <= nAllowQueueFrame) {
		// 得到需要填充的声卡缓存指针 => 已经计算出要填充的帧数...
		hr = m_render->GetBuffer(resample_frames, &output);
		if (SUCCEEDED(hr) && output != NULL) {
			// 将数据填充到指定的缓存，并投递到声卡播放...
			memcpy(output, out_buffer_ptr, out_buffer_size);
			hr = m_render->ReleaseBuffer(resample_frames, 0);
		}
	}
	// 打印正在播放的解码后音频数据...
	//blog(LOG_INFO, "%s [Audio] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, QueueFrame: %d/%d",
	//	TM_RECV_NAME, frame_pts_ms, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), nCurPadFrame, nAllowQueueFrame);
	// 删除已经使用的音频数据 => 从环形队列中移除...
	circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
	// 减少环形队列中有效数据帧的个数...
	--m_frame_num;
	// 已经有播放，不能休息...
	m_bNeedSleep = false;
}

bool CAudioPlay::doVolumeEvent(bool bIsVolPlus)
{
	float fNewVolRate = m_fVolRate + (bIsVolPlus ? 0.5f : -0.5f);
	fNewVolRate = ((fNewVolRate >= 6.0f) ? 6.0f : fNewVolRate);
	fNewVolRate = ((fNewVolRate <= 1.0f) ? 1.0f : fNewVolRate);
	m_fVolRate = fNewVolRate;
	return true;
}

void CAudioPlay::doSleepTo()
{
	// < 0 不能休息，有不能休息标志 => 都直接返回...
	if (!m_bNeedSleep || m_play_next_ns < 0)
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// 如果比当前时间小(已过期)，直接返回...
	if (m_play_next_ns <= cur_time_ns)
		return;
	// 计算超前时间的差值，最多休息10毫秒...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// 调用系统工具函数，进行sleep休息...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

void CAudioPlay::Entry()
{
	while (!this->IsStopRequested()) {
		// 设置休息标志 => 只要有解码或播放就不能休息...
		m_bNeedSleep = true;
		// 解码一帧音频...
		this->doDecodeFrame();
		// 显示一帧音频...
		this->doDisplayAudio();
		// 进行sleep等待...
		this->doSleepTo();
	}
}