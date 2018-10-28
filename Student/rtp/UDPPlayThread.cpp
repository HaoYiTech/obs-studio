
#include "student-app.h"
#include "BitWritter.h"
#include "UDPPlayThread.h"
#include "../window-view-render.hpp"

CDecoder::CDecoder()
  : m_lpCodec(NULL)
  , m_lpDFrame(NULL)
  , m_lpDecoder(NULL)
  , m_play_next_ns(-1)
  , m_bNeedSleep(false)
{
	// 初始化解码器互斥对象...
	pthread_mutex_init_value(&m_DecoderMutex);
}

CDecoder::~CDecoder()
{
	// 释放解码结构体...
	if( m_lpDFrame != NULL ) {
		av_frame_free(&m_lpDFrame);
		m_lpDFrame = NULL;
	}
	// 释放解码器对象...
	if( m_lpDecoder != NULL ) {
		avcodec_close(m_lpDecoder);
		av_free(m_lpDecoder);
	}
	// 释放队列中的解码前的数据块...
	GM_MapPacket::iterator itorPack;
	for(itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// 释放没有播放完毕的解码后的数据帧...
	GM_MapFrame::iterator itorFrame;
	for(itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
	// 释放解码器互斥对象...
	pthread_mutex_destroy(&m_DecoderMutex);
}

void CDecoder::doPushPacket(AVPacket & inPacket)
{
	// 注意：这里必须以DTS排序，决定了解码的先后顺序...
	// 如果有相同DTS的数据帧已经存在，直接释放AVPacket，返回...
	if( m_MapPacket.find(inPacket.dts) != m_MapPacket.end() ) {
		blog(LOG_INFO, "%s Error => SameDTS: %I64d, StreamID: %d", TM_RECV_NAME, inPacket.dts, inPacket.stream_index);
		av_free_packet(&inPacket);
		return;
	}
	// 如果没有相同DTS的数据帧，保存起来...
	m_MapPacket[inPacket.dts] = inPacket;
}

void CDecoder::doSleepTo()
{
	// < 0 不能休息，有不能休息标志 => 都直接返回...
	if( !m_bNeedSleep || m_play_next_ns < 0 )
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// 如果比当前时间小(已过期)，直接返回...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// 计算超前时间的差值，最多休息10毫秒...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// 调用系统工具函数，进行sleep休息...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

CVideoThread::CVideoThread(CPlaySDL * lpPlaySDL, CViewRender * lpViewRender)
  : m_lpViewRender(lpViewRender)
  , m_lpPlaySDL(lpPlaySDL)
  , m_nSDLTextureHeight(0)
  , m_nSDLTextureWidth(0)
  , m_img_buffer_ptr(NULL)
  , m_img_buffer_size(0)
  , m_sdlRenderer(NULL)
  , m_sdlTexture(NULL)
  , m_sdlScreen(NULL)
{
	ASSERT(m_lpViewRender != NULL);
	ASSERT( m_lpPlaySDL != NULL );
}

CVideoThread::~CVideoThread()
{
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 销毁SDL窗口...
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// 销毁SDL窗口时会隐藏关联窗口...
	if( m_lpViewRender != NULL ) {
		HWND hWnd = m_lpViewRender->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
	}
	// 释放单帧图像转换空间...
	if( m_img_buffer_ptr != NULL ) {
		av_free(m_img_buffer_ptr);
		m_img_buffer_ptr = NULL;
		m_img_buffer_size = 0;
	}
}

// 判断SDL是否需要重建...
bool CVideoThread::IsCanRebuildSDL(int nPicWidth, int nPicHeight)
{
	// 纹理的宽高发生变化，需要重建，需要清理窗口变化，避免重复创建...
	if (m_nSDLTextureWidth != nPicWidth || m_nSDLTextureHeight != nPicHeight) {
		m_lpViewRender->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_nSDLTextureWidth == nPicWidth && m_nSDLTextureHeight == nPicHeight);
	// SDL相关对象无效，需要重建，需要清理窗口变化，避免重复创建...
	if (m_sdlScreen == NULL || m_sdlRenderer == NULL || m_sdlTexture == NULL) {
		m_lpViewRender->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_sdlScreen != NULL && m_sdlRenderer != NULL && m_sdlTexture != NULL);
	return m_lpViewRender->GetAndResetRenderFlag();
}

void CVideoThread::doReBuildSDL(int nPicWidth, int nPicHeight)
{
	// 销毁SDL窗口 => 会自动隐藏关联窗口...
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	// 销毁渲染器...
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	// 销毁纹理...
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// 重建SDL相关对象...
	if( m_lpViewRender != NULL ) {
		// 销毁SDL窗口时会隐藏关联窗口 => 必须用Windows的API接口...
		HWND hWnd = m_lpViewRender->GetRenderHWnd();
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

BOOL CVideoThread::InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	///////////////////////////////////////////////////////////////////////
	// 注意：这里丢弃了nWidth|nHeight|nFPS，使用解码之后的图片高宽更精确...
	///////////////////////////////////////////////////////////////////////
	// 如果已经初始化，直接返回...
	if( m_lpCodec != NULL || m_lpDecoder != NULL )
		return false;
	ASSERT( m_lpCodec == NULL && m_lpDecoder == NULL );
	// 保存传递过来的参数信息...
	m_strSPS = inSPS;
	m_strPPS = inPPS;
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// 查找需要的解码器 => 不用创建解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 设置支持低延时模式 => 没啥作用，必须配合具体过程...
	m_lpDecoder->flags |= CODEC_FLAG_LOW_DELAY;
	// 设置支持不完整片段解码 => 没啥作用...
	if( m_lpCodec->capabilities & CODEC_CAP_TRUNCATED ) {
		m_lpDecoder->flags |= CODEC_FLAG_TRUNCATED;
	}
	// 设置解码器关联参数配置 => 这里设置不起作用...
	//m_lpDecoder->refcounted_frames = 1;
	//m_lpDecoder->has_b_frames = 0;
	// 打开获取到的解码器...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) < 0 ) {
		blog(LOG_INFO, "%s [Video] avcodec_open2 failed.", TM_RECV_NAME);
		return false;
	}
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDFrame = av_frame_alloc();
	ASSERT( m_lpDFrame != NULL );
	// 启动线程开始运转...
	this->Start();
	return true;
}

void CVideoThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 设置休息标志 => 只要有解码或播放就不能休息...
		m_bNeedSleep = true;
		// 解码一帧视频...
		this->doDecodeFrame();
		// 显示一帧视频...
		this->doDisplaySDL();
		// 进行sleep等待...
		this->doSleepTo();
	}
}

void CVideoThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// 线程正在退出中，直接返回...
	if( this->IsStopRequested() )
		return;
	// 每个关键帧都放入sps和pps，播放会加快...
	string strCurFrame;
	// 如果是关键帧，必须先写入sps，再写如pps...
	if( bIsKeyFrame ) {
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
	AVPacket  theNewPacket = {0};
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
	pthread_mutex_lock(&m_DecoderMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_DecoderMutex);
}
//
// 取出一帧解码后的视频，比对系统时间，看看能否播放...
void CVideoThread::doDisplaySDL()
{
	/////////////////////////////////////////////////////////////////
	// 注意：这里产生的AVFrame是同一个线程顺序执行，无需互斥...
	/////////////////////////////////////////////////////////////////

	// 如果没有已解码数据帧，直接返回休息最大毫秒数...
	if( m_MapFrame.size() <= 0 ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 这是为了测试原始PTS而获取的初始PTS值 => 只在打印调试信息时使用...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// 计算当前时刻点与系统0点位置的时间差 => 转换成毫秒...
	int64_t sys_cur_ms = (os_gettime_ns() - m_lpPlaySDL->GetSysZeroNS())/1000000;
	// 累加人为设定的延时毫秒数 => 相减...
	sys_cur_ms -= m_lpPlaySDL->GetZeroDelayMS();
	// 取出第一个已解码数据帧 => 时间最小的数据帧...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame = itorItem->second;
	int64_t   frame_pts_ms = itorItem->first;
	// 当前帧的显示时间还没有到 => 直接休息差值...
	if( frame_pts_ms > sys_cur_ms ) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms)*1000000;
		//blog(LOG_INFO, "%s [Video] Advance => PTS: %I64d, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, frame_pts_ms - sys_cur_ms, m_MapPacket.size(), m_MapFrame.size());
		return;
	}
	// 将数据转换成jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Dst");
	// 打印正在播放的解码后视频数据...
	//blog(LOG_INFO, "%s [Video] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), m_MapFrame.size());
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：视频延时帧（落后帧），不能丢弃，必须继续显示，视频消耗速度相对较快，除非时间戳给错了，会造成播放问题。
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 准备需要转换的格式信息...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	// 解码后的高宽，可能与预设的高宽不一致...
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	int nDstWidth = m_lpDecoder->width;   // 没有使用预设值m_nDstWidth，海康的rtsp获取高宽有问题;
	int nDstHeight = m_lpDecoder->height; // 没有使用预设值m_nDstHeight，海康的rtsp获取高宽有问题;
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
	AVPicture pDestFrame = {0};
	avpicture_fill(&pDestFrame, m_img_buffer_ptr, nDestFormat, nDstWidth, nDstHeight);
	// 另一种方法：临时分配图像格式转换空间的方法...
	//int nDestBufSize = avpicture_get_size(nDestFormat, nDstWidth, nDstHeight);
	//uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	//avpicture_fill(&pDestFrame, pDestOutBuf, nDestFormat, nDstWidth, nDstHeight);
	// 调用ffmpeg的格式转换接口函数...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nDstWidth, nDstHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame.data, pDestFrame.linesize);
	sws_freeContext(img_convert_ctx);
	//////////////////////////////////////////////////////////////////////////////////
	// 使用SDL 进行画面绘制工作 => 正在处理全屏时，不能绘制，会与D3D发生冲突崩溃...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_lpViewRender != NULL && !m_lpViewRender->IsChangeScreen()) {
		// 用解码后图像高宽去判断是否需要重建SDL，播放窗口发生变化也要重建...
		if (this->IsCanRebuildSDL(nSrcWidth, nSrcHeight)) {
			this->doReBuildSDL(nSrcWidth, nSrcHeight);
		}
		// 获取渲染窗口的矩形区域...
		QRect & rcRect = m_lpViewRender->GetRenderRect();
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
		int nResult = SDL_UpdateTexture( m_sdlTexture, &srcSdlRect, pDestFrame.data[0], pDestFrame.linesize[0] );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		nResult = SDL_RenderClear( m_sdlRenderer );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
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
		nResult = SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// 正式激发渲染窗口，绘制出图像来...
		SDL_RenderPresent( m_sdlRenderer );
	}
	// 释放临时分配的数据空间...
	//av_free(pDestOutBuf);
	// 释放并删除已经使用完毕原始数据块...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
	// 修改休息状态 => 已经有播放，不能休息...
	m_bNeedSleep = false;
}

#ifdef DEBUG_DECODE
static void DoSaveLocFile(AVFrame * lpAVFrame, bool bError, AVPacket & inPacket)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Src/loc.obj";
	FILE * pFile = fopen(lpszPath, "a+");
	if( bError ) {
		fwrite(&inPacket.pts, 1, sizeof(int64_t), pFile);
		fwrite(inPacket.data, 1, inPacket.size, pFile);
	} else {
		fwrite(&lpAVFrame->best_effort_timestamp, 1, sizeof(int64_t), pFile);
		for(int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
			if( lpAVFrame->data[i] != NULL && lpAVFrame->linesize[i] > 0 ) {
				fwrite(lpAVFrame->data[i], 1, lpAVFrame->linesize[i], pFile);
			}
		}
	}
	fclose(pFile);
}
static void DoSaveNetFile(AVFrame * lpAVFrame, bool bError, AVPacket & inPacket)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Src/net.obj";
	FILE * pFile = fopen(lpszPath, "a+");
	if( bError ) {
		fwrite(&inPacket.pts, 1, sizeof(int64_t), pFile);
		fwrite(inPacket.data, 1, inPacket.size, pFile);
	} else {
		fwrite(&lpAVFrame->best_effort_timestamp, 1, sizeof(int64_t), pFile);
		for(int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
			if( lpAVFrame->data[i] != NULL && lpAVFrame->linesize[i] > 0 ) {
				fwrite(lpAVFrame->data[i], 1, lpAVFrame->linesize[i], pFile);
			}
		}
	}
	fclose(pFile);
}
#endif // DEBUG_DECODE

void CVideoThread::doDecodeFrame()
{
	// 没有要解码的数据包，直接返回最大休息毫秒数...
	if( m_MapPacket.size() <= 0 ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 这是为了测试原始PTS而获取的初始PTS值 => 只在打印调试信息时使用...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// 抽取一个AVPacket进行解码操作，理论上一个AVPacket一定能解码出一个Picture...
	// 由于数据丢失或B帧，投递一个AVPacket不一定能返回Picture，这时解码器就会将数据缓存起来，造成解码延时...
	int got_picture = 0, nResult = 0;
	// 为了快速解码，只要有数据就解码，同时与数据输入线程互斥...
	pthread_mutex_lock(&m_DecoderMutex);
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_DecoderMutex);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：由于只有 I帧 和 P帧，没有B帧，要让解码器快速丢掉解码错误数据，通过设置has_b_frames来完成
	// 技术文档 => https://bbs.csdn.net/topics/390692774
	//////////////////////////////////////////////////////////////////////////////////////////////////
	m_lpDecoder->has_b_frames = 0;
	// 将完整压缩数据帧放入解码器进行解码操作...
	nResult = avcodec_decode_video2(m_lpDecoder, m_lpDFrame, &got_picture, &thePacket);
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：目前只有 I帧 和 P帧  => 这里使用全局AVFrame，非常重要，能提供后续AVPacket的解码支持...
	// 解码失败或没有得到完整图像 => 是需要后续AVPacket的数据才能解码出图像...
	// 非常关键的操作 => m_lpDFrame 千万不能释放，继续灌AVPacket就能解码出图像...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if( nResult < 0 || !got_picture ) {
		// 保存解码失败的情况...
#ifdef DEBUG_DECODE
		(m_lpRenderWnd != NULL) ? DoSaveNetFile(m_lpDFrame, true, thePacket) : DoSaveLocFile(m_lpDFrame, true, thePacket);
#endif // DEBUG_DECODE
		// 打印解码失败信息，显示坏帧的个数...
		//blog(LOG_INFO, "%s [Video] %s Error => decode_frame failed, BFrame: %d, PTS: %I64d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, (m_lpRenderWnd != NULL) ? "net" : "loc", m_lpDecoder->has_b_frames, thePacket.pts + inStartPtsMS, nResult, thePacket.size);
		// 这里非常关键，告诉解码器不要缓存坏帧(B帧)，一旦有解码错误，直接扔掉，这就是低延时解码模式...
		m_lpDecoder->has_b_frames = 0;
		// 丢掉解码失败的数据帧...
		av_free_packet(&thePacket);
		return;
	}
	// 如果调试解码器，进行数据存盘...
#ifdef DEBUG_DECODE
	(m_lpRenderWnd != NULL) ? DoSaveNetFile(m_lpDFrame, false, thePacket) : DoSaveLocFile(m_lpDFrame, false, thePacket);
#endif // DEBUG_DECODE
	// 打印解码之后的数据帧信息...
	//blog(LOG_INFO, "%s [Video] Decode => BFrame: %d, PTS: %I64d, pkt_dts: %I64d, pkt_pts: %I64d, Type: %d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, m_lpDecoder->has_b_frames,
	//		m_lpDFrame->best_effort_timestamp + inStartPtsMS, m_lpDFrame->pkt_dts + inStartPtsMS, m_lpDFrame->pkt_pts + inStartPtsMS,
	//		m_lpDFrame->pict_type, nResult, thePacket.size );
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里使用AVFrame计算的最佳有效时间戳，使用AVPacket的时间戳也是一样的...
	// 因为，采用了低延时的模式，坏帧都扔掉了，解码器内部没有缓存数据，不会出现时间戳错位问题...
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = m_lpDFrame->best_effort_timestamp;
	// 重新克隆AVFrame，自动分配空间，按时间排序...
	m_MapFrame[frame_pts_ms] = av_frame_clone(m_lpDFrame);
	//DoProcSaveJpeg(m_lpDFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// 这里需要释放AVPacket的缓存...
	av_free_packet(&thePacket);
	// 修改休息状态 => 已经有解码，不能休息...
	m_bNeedSleep = false;

	/*// 计算解码后的数据帧的时间戳 => 将毫秒转换成纳秒...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);*/
}

#define ACTUALLY_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

ACTUALLY_DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
ACTUALLY_DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
ACTUALLY_DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
ACTUALLY_DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

CAudioThread::CAudioThread(CPlaySDL * lpPlaySDL)
{
	m_device = NULL;
	m_client = NULL;
	m_render = NULL;
	m_frame_num = 0;
	m_lpPlaySDL = lpPlaySDL;
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	m_max_buffer_size = 0;
	m_max_buffer_ptr = NULL;
	m_out_convert_ctx = NULL;
	// 默认输出声道、输出采样率...
	m_out_channel_num = 1;
	m_out_sample_rate = 8000;
	m_out_frame_bytes = 0;
	m_out_frame_duration = 0;
	// SDL需要的是AV_SAMPLE_FMT_S16，WSAPI需要的是AV_SAMPLE_FMT_FLT...
	m_out_sample_fmt = AV_SAMPLE_FMT_FLT;
	// 初始化PCM数据环形队列...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE/4);
}

CAudioThread::~CAudioThread()
{
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 关闭音频转换对象...
	if (m_out_convert_ctx != NULL) {
		swr_free(&m_out_convert_ctx);
		m_out_convert_ctx = NULL;
	}
	// 关闭单帧最大缓冲区...
	if (m_max_buffer_ptr != NULL) {
		av_free(m_max_buffer_ptr);
		m_max_buffer_ptr = NULL;
		m_max_buffer_size = 0;
	}
	// 释放音频环形队列...
	circlebuf_free(&m_circle);

	// 关闭音频监听设备...
	this->doMonitorFree();

	/*// 关闭音频设备...
	SDL_CloseAudio();
	m_nDeviceID = 0;*/
}

#ifdef DEBUG_AEC
static void DoSaveTeacherPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel)
{
	// 注意：PCM数据必须用二进制方式打开文件...
	static char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "F:/MP4/PCM/Teacher_%d_%d.pcm", nAudioRate, nAudioChannel);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// 打开文件成功，开始写入音频PCM数据内容...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}
#endif // DEBUG_AEC

void CAudioThread::doDecodeFrame()
{
	// 没有要解码的数据包，直接返回最大休息毫秒数...
	if( m_MapPacket.size() <= 0 || m_render == NULL ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 这是为了测试原始PTS而获取的初始PTS值 => 只在打印调试信息时使用...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// 抽取一个AVPacket进行解码操作，一个AVPacket不一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	// 为了快速解码，只要有数据就解码，同时与数据输入线程互斥...
	pthread_mutex_lock(&m_DecoderMutex);
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_DecoderMutex);
	// 注意：这里解码后的格式是4bit，需要转换成16bit，调用swr_convert
	nResult = avcodec_decode_audio4(m_lpDecoder, m_lpDFrame, &got_picture, &thePacket);
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里使用全局AVFrame，非常重要，能提供后续AVPacket的解码支持...
	// 解码失败或没有得到完整图像 => 是需要后续AVPacket的数据才能解码出图像...
	// 非常关键的操作 => m_lpDFrame 千万不能释放，继续灌AVPacket就能解码出图像...
	////////////////////////////////////////////////////////////////////////////////////////////////
	if( nResult < 0 || !got_picture ) {
		blog(LOG_INFO, "%s [Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, thePacket.pts + inStartPtsMS, nResult, thePacket.size);
		if( nResult < 0 ) {
			static char szErrBuf[64] = {0};
			av_strerror(nResult, szErrBuf, 64);
			blog(LOG_INFO, "%s [Audio] Error => %s ", TM_RECV_NAME, szErrBuf);
		}
		av_free_packet(&thePacket);
		return;
	}
	// 打印解码之后的数据帧信息...
	//blog(LOG_INFO, "%s [Audio] Decode => PTS: %I64d, pkt_dts: %I64d, pkt_pts: %I64d, Type: %d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME,
	//		m_lpDFrame->best_effort_timestamp + inStartPtsMS, m_lpDFrame->pkt_dts + inStartPtsMS, m_lpDFrame->pkt_pts + inStartPtsMS,
	//		m_lpDFrame->pict_type, nResult, thePacket.size );
	////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里仍然使用AVPacket的解码时间轴做为播放时间...
	// 这样，一旦由于解码失败造成的数据中断，也不会造成音频播放的延迟问题...
	////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = thePacket.pts;
	// 对解码后的音频进行格式转换...
	this->doConvertAudio(frame_pts_ms, m_lpDFrame);
	// 这里需要释放AVPacket的缓存...
	av_free_packet(&thePacket);
	// 修改休息状态 => 已经有播放，不能休息...
	m_bNeedSleep = false;
}

void CAudioThread::doConvertAudio(int64_t in_pts_ms, AVFrame * lpDFrame)
{
	// 输入声道和输出声道 => 转换成单声道...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	// 输入输出的音频格式...
	AVSampleFormat in_sample_fmt = m_lpDecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = m_out_sample_fmt;
	// 设置输入输出采样率 => 转换成8K采样率...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	// 注意：已经开始转换，swr_get_delay()将返回上次转换遗留的样本数，因此，要准备比正常样本大的空间...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + lpDFrame->nb_samples;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// 获取音频解码后输出的缓冲区大小 => 分配新的空间，由于有上次转换遗留样本，需要的空间会有变化...
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	// 如果新需要的空间大于了当前单帧最大空间，需要重建空间...
	if (out_buffer_size > m_max_buffer_size) {
		// 删除之前分配的单帧最大空间...
		if (m_max_buffer_ptr != NULL) {
			av_free(m_max_buffer_ptr);
			m_max_buffer_ptr = NULL;
		}
		// 重新分配最新的单帧最大空间...
		m_max_buffer_size = out_buffer_size;
		m_max_buffer_ptr = (uint8_t *)av_malloc(out_buffer_size);
	}
	// 调用音频转换接口，转换音频数据 => 返回值很重要，它是实际获取的已转换的样本数 => 使用最大空间去接收转换后的数据内容...
	int nResult = swr_convert(m_out_convert_ctx, &m_max_buffer_ptr, m_max_buffer_size, (const uint8_t **)lpDFrame->data, lpDFrame->nb_samples);
	// 转换失败，直接返回 => 没有需要抽取的转换后数据内容...
	if (nResult <= 0) return;
	// 需要将转换后的样本数，转换成实际的缓存大小，这就能解释为啥，在SDL播放时，总是会出现数据无法播放完的问题，因为，给了太多数据...
	int cur_data_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, nResult, out_sample_fmt, 1);
	// 将获取到缓存放入缓冲队列当中 => PTS|Size|Data => 从最大单帧空间中抽取本次转换的有效数据内容...
	circlebuf_push_back(&m_circle, &in_pts_ms, sizeof(int64_t));
	circlebuf_push_back(&m_circle, &cur_data_size, sizeof(int));
	circlebuf_push_back(&m_circle, m_max_buffer_ptr, cur_data_size);
	//blog(LOG_INFO, "[Audio] HornPCM => PTS: %I64d, Size: %d", in_pts_ms, cur_data_size);
	// 累加环形队列中有效数据帧的个数...
	++m_frame_num;
#ifdef DEBUG_AEC
	// 注意：格式统一成8K采样率，单声道...
	DoSaveTeacherPCM(m_max_buffer_ptr, cur_data_size, m_out_sample_rate, m_out_channel_num);
#endif // DEBUG_AEC
}

void CAudioThread::doDisplaySDL()
{
	//////////////////////////////////////////////////////////////////
	// 注意：使用主动投递方式，可以有效降低回调造成的延时...
	// 注意：这里产生的PCM数据是同一个线程顺序执行，无需互斥...
	/////////////////////////////////////////////////////////////////

	/*////////////////////////////////////////////////////////////////
	// 测试：这里可以明显看出，网络抖动之后，后面数据一下子就被灌入，
	// 但是，音频播放并不会因此加速播放，卡顿时间被累积，音频被拉长，
	// 后续数据就发生堆积，累积延时增大，引发音频清理，造成咔哒声音，
	// 因此，需要找到能够精确控制音频播放的工具，不能只是扔给硬件了事
	//////////////////////////////////////////////////////////////////
	if( m_nDeviceID > 0 ) {
		blog(LOG_INFO, "%s [Audio] QueueBytes => %d, circle: %lu", TM_RECV_NAME, SDL_GetQueuedAudioSize(m_nDeviceID), m_circle.size);
	}*/

	// 如果没有已解码数据帧，直接返回最大休息毫秒数...
	if( m_circle.size <= 0 || m_render == NULL ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// 这是为了测试原始PTS而获取的初始PTS值 => 只在打印调试信息时使用...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// 计算当前时间与0点位置的时间差 => 转换成毫秒...
	int64_t sys_cur_ms = (os_gettime_ns() - m_lpPlaySDL->GetSysZeroNS())/1000000;
	// 累加人为设定的延时毫秒数 => 相减...
	sys_cur_ms -= m_lpPlaySDL->GetZeroDelayMS();
	// 获取第一个已解码数据帧 => 时间最小的数据帧...
	int64_t frame_pts_ms = 0; int out_buffer_size = 0;
	circlebuf_peek_front(&m_circle, &frame_pts_ms, sizeof(int64_t));
	// 不能超前投递数据，会造成硬件层数据堆积，造成缓存积压，引发缓存清理...
	if( frame_pts_ms > sys_cur_ms ) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms)*1000000;
		return;
	}
	// 当前帧时间戳从环形队列当中移除 => 后面是数据帧长度...
	circlebuf_pop_front(&m_circle, NULL, sizeof(int64_t));
	// 从环形队列当中，取出音频数据帧内容，动态长度...
	circlebuf_pop_front(&m_circle, &out_buffer_size, sizeof(int));
	// 当前数据帧长度一定小于或等于最大单帧输出空间...
	ASSERT(m_max_buffer_size >= out_buffer_size);
	// 从环形队列读取当前帧内容，因为是顺序执行，可以再次使用单帧最大输出空间...
	circlebuf_peek_front(&m_circle, m_max_buffer_ptr, out_buffer_size);
	
	// 计算当前数据块包含的有效采样个数...
	int nPerFrameSize = (m_out_channel_num * sizeof(float));
	uint32_t resample_frames = out_buffer_size / nPerFrameSize;
	
	// 设置音量数据的转换 => 这里进行音量的放大...
	float vol = m_lpPlaySDL->GetVolRate();
	if (!close_float(vol, 1.0f, EPSILON)) {
		register float *cur = (float*)m_max_buffer_ptr;
		register float *end = cur + resample_frames * m_out_channel_num;
		while (cur < end) {
			*(cur++) *= vol;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：必须对音频播放内部的缓存做定期伐值清理 => CPU过高时，DirectSound会堆积缓存...
	// 投递数据前，先查看正在排队的音频数据 => 缓存超过500毫秒就清理 => 计算出500毫秒包含的总帧数(总字节数)...
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//int nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	/*int nAllowQueueSize = (int)(500.0f / m_out_frame_duration * m_out_frame_bytes);
	// 清理之后，立即继续灌音频数据，避免数据进一步堆积...
	if( nQueueBytes > nAllowQueueSize ) {
		blog(LOG_INFO, "%s [Audio] Clear Audio Buffer, QueueBytes: %d, AVPacket: %d, AVFrame: %d", TM_RECV_NAME, nQueueBytes, m_MapPacket.size(), m_frame_num);
		SDL_ClearQueuedAudio(m_nDeviceID);
	}
	//blog(LOG_INFO, "[Audio] Delay: %d ms", nQueueBytes / 16);
	// 注意：失败也不要返回，继续执行，目的是移除缓存...
	// 将音频解码后的数据帧投递给音频设备...
	if( SDL_QueueAudio(m_nDeviceID, m_max_buffer_ptr, out_buffer_size) < 0 ) {
		blog(LOG_INFO, "%s [Audio] Failed to queue audio: %s", TM_RECV_NAME, SDL_GetError());
	}*/
	
	HRESULT hr = S_OK;
	BYTE * output = NULL;
	UINT32 nCurPadFrame = 0;
	UINT32 nAllowQueueFrame = 0;
	int    msInSndCardBuf = 0;
	// 获取当前声卡已缓存的帧数量...
	hr = m_client->GetCurrentPadding(&nCurPadFrame);
	// 计算在声卡中已缓存的毫秒数 => 向下取整...
	msInSndCardBuf = (int)((nCurPadFrame * 1.0f * nPerFrameSize / m_out_frame_bytes) * m_out_frame_duration + 0.5f);
	// 将400毫秒的声卡缓存，转换成帧数量...
	nAllowQueueFrame = (300.0f / m_out_frame_duration * m_out_frame_bytes) / nPerFrameSize;
	// 只有当声卡缓存小于300毫秒时，才进行数据投递，大于300毫秒，直接丢弃...
	if (nCurPadFrame <= nAllowQueueFrame) {
		// 得到需要填充的声卡缓存指针 => 已经计算出要填充的帧数...
		hr = m_render->GetBuffer(resample_frames, &output);
		if (SUCCEEDED(hr) && output != NULL) {
			// 将数据填充到指定的缓存，并投递到声卡播放...
			memcpy(output, m_max_buffer_ptr, out_buffer_size);
			hr = m_render->ReleaseBuffer(resample_frames, 0);
			// 把解码后的音频数据投递给当前正在被拉取的通道进行回音消除...
			App()->doEchoCancel(m_max_buffer_ptr, out_buffer_size, m_out_sample_rate, m_out_channel_num, msInSndCardBuf);
		}
	} else {
		// 注意：经过试验，丢弃或保持原样，都会降低AEC效果...
		// 声卡缓存大于300毫秒，重置静音，打印调试信息...
		memset(m_max_buffer_ptr, out_buffer_size, 0);
		App()->doEchoCancel(m_max_buffer_ptr, out_buffer_size, m_out_sample_rate, m_out_channel_num, msInSndCardBuf);
		blog(LOG_INFO, "%s [Audio] Delayed, Padding: %u, AVPacket: %d, AVFrame: %d", TM_RECV_NAME, nCurPadFrame, m_MapPacket.size(), m_frame_num);
	}
	// 删除已经使用的音频数据 => 从环形队列中移除...
	circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
	// 减少环形队列中有效数据帧的个数...
	--m_frame_num;
	// 修改休息状态 => 已经有播放，不能休息...
	m_bNeedSleep = false;
}

BOOL CAudioThread::InitAudio(int nInRateIndex, int nInChannelNum)
{
	// 如果已经初始化，直接返回...
	if( m_lpCodec != NULL || m_lpDecoder != NULL )
		return false;
	ASSERT( m_lpCodec == NULL && m_lpDecoder == NULL );
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
	switch(nInRateIndex)
	{
	case 0x03: m_in_sample_rate = 48000; break;
	case 0x04: m_in_sample_rate = 44100; break;
	case 0x05: m_in_sample_rate = 32000; break;
	case 0x06: m_in_sample_rate = 24000; break;
	case 0x07: m_in_sample_rate = 22050; break;
	case 0x08: m_in_sample_rate = 16000; break;
	case 0x09: m_in_sample_rate = 12000; break;
	case 0x0a: m_in_sample_rate = 11025; break;
	case 0x0b: m_in_sample_rate =  8000; break;
	default:   m_in_sample_rate = 48000; break;
	}
	// 保存输入采样率索引和声道...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	// 保存输出采样率和输出声道...
	//m_out_sample_rate = nOutSampleRate;
	//m_out_channel_num = nOutChannelNum;
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 打开获取到的解码器...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// 准备一个全局的解码结构体 => 解码数据帧是相互关联的...
	m_lpDFrame = av_frame_alloc();
	ASSERT(m_lpDFrame != NULL);
	// 输入声道和输出声道 => 转换成单声道...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输入输出的音频格式...
	AVSampleFormat in_sample_fmt = m_lpDecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = m_out_sample_fmt;
	// 设置输入输出采样率 => 转换成8K采样率...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// 分配并初始化转换器...
	m_out_convert_ctx = swr_alloc();
	m_out_convert_ctx = swr_alloc_set_opts(m_out_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, in_sample_fmt, in_sample_rate, 0, NULL);
	// 初始化转换器失败，打印信息，返回错误...
	if (AVERROR(swr_init(m_out_convert_ctx)) < 0) {
		blog(LOG_INFO, "error => swr_init");
		return false;
	}

	// 输入输出音频采样个数 => AAC-1024 | MP3-1152
	// 注意：还没有开始转换，swr_get_delay()返回0，out_nb_samples是正常的样本数，不会变...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + 1024;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// 计算输出每帧持续时间(毫秒)，每帧字节数 => 只需要计算一次就够了...
	m_out_frame_duration = out_nb_samples * 1000 / out_sample_rate;
	m_out_frame_bytes = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);

	// 通过所有通道扬声器创建成功...
	App()->SetAudioHorn(true);

	/*//SDL_AudioSpec => 不能使用系统推荐参数 => 不用回调模式，主动投递...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS;
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = NULL; 
	audioSpec.userdata = NULL;
	audioSpec.silence = 0;
	// 打开SDL音频设备 => 只能打开一个设备...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		//SDL_Log("[Audio] Failed to open audio: %s", SDL_GetError());
		blog(LOG_INFO, "%s [Audio] Failed to open audio: %s", TM_RECV_NAME, SDL_GetError());
		return false;
	}
	// 打开的主设备编号...
	m_nDeviceID = 1;
	// 开始播放 => 使用默认主设备...
	SDL_PauseAudioDevice(m_nDeviceID, 0);
	SDL_ClearQueuedAudio(m_nDeviceID);*/

	// 启动线程...
	this->Start();
	return true;
}

void CAudioThread::doMonitorFree()
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
	// 通过所有通道扬声器释放完成...
	App()->SetAudioHorn(false);
}

void CAudioThread::Entry()
{
	// 注意：提高线程优先级，并不能解决音频受系统干扰问题...
	// 设置线程优先级 => 最高级，防止外部干扰...
	//if( !::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST) ) {
	//	blog(LOG_INFO, "[Audio] SetThreadPriority to THREAD_PRIORITY_HIGHEST failed.");
	//}
	while( !this->IsStopRequested() ) {
		// 设置休息标志 => 只要有解码或播放就不能休息...
		m_bNeedSleep = true;
		// 解码一帧视频...
		this->doDecodeFrame();
		// 显示一帧视频...
		this->doDisplaySDL();
		// 进行sleep等待...
		this->doSleepTo();
	}
}
//
// 需要对音频帧数据添加头信息...
void CAudioThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// 线程正在退出中，直接返回...
	if (this->IsStopRequested())
		return;
	// 先加入ADTS头，再加入数据帧内容...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// 构造ADTS帧头...
	PutBitContext pb;
	char pbuf[ADTS_HEADER_SIZE * 2] = {0};
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
	AVPacket  theNewPacket = {0};
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
	pthread_mutex_lock(&m_DecoderMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_DecoderMutex);
}

CPlaySDL::CPlaySDL(CViewRender * lpViewRender, int64_t inSysZeroNS)
  : m_lpViewRender(lpViewRender)
  , m_sys_zero_ns(inSysZeroNS)
  , m_bFindFirstVKey(false)
  , m_lpVideoThread(NULL)
  , m_lpAudioThread(NULL)
  , m_zero_delay_ms(-1)
  , m_start_pts_ms(-1)
  , m_fVolRate(1.0f)
{
	ASSERT( m_lpViewRender != NULL );
	ASSERT( m_sys_zero_ns > 0 );
	// 初始化SDL2.0 => QT线程当中已经调用了 CoInitializeEx()...
	int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
}

CPlaySDL::~CPlaySDL()
{
	// 释放音视频解码对象...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	// 释放SDL2.0资源...
	SDL_Quit();
}

bool CPlaySDL::doVolumeEvent(bool bIsVolPlus)
{
	float fNewVolRate = m_fVolRate + (bIsVolPlus ? 0.5f : -0.5f);
	fNewVolRate = ((fNewVolRate >= 6.0f) ? 6.0f : fNewVolRate);
	fNewVolRate = ((fNewVolRate <= 1.0f) ? 1.0f : fNewVolRate);
	m_fVolRate = fNewVolRate;
	return true;
}

BOOL CPlaySDL::InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// 创建新的视频对象...
	if (m_lpVideoThread != NULL) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	// 创建视频线程对象...
	m_lpVideoThread = new CVideoThread(this, m_lpViewRender);
	// 初始化视频对象失败，直接删除视频对象，返回失败...
	if (!m_lpVideoThread->InitVideo(inSPS, inPPS, nWidth, nHeight, nFPS)){
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
		return false;
	}
	// 返回成功...
	return true;
}

BOOL CPlaySDL::InitAudio(int nInRateIndex, int nInChannelNum)
{
	// 创建新的音频对象...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	// 创建音频线程对象...
	m_lpAudioThread = new CAudioThread(this);
	// 初始化音频对象失败，直接删除音频对象，返回失败...
	if (!m_lpAudioThread->InitAudio(nInRateIndex, nInChannelNum)) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
		return false;
	}
	// 返回成功...
	return true;
}

void CPlaySDL::PushPacket(int zero_delay_ms, string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime)
{
	// 为了解决突发延时抖动，要用一种遗忘衰减算法，进行播放延时控制...
	// 直接使用计算出的缓存时间设定延时时间 => 缓存就是延时...
	if( m_zero_delay_ms < 0 ) { m_zero_delay_ms = zero_delay_ms; }
	else { m_zero_delay_ms = (7 * m_zero_delay_ms + zero_delay_ms) / 8; }

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：不能在这里设置系统0点时刻，必须在之前设定0点时刻...
	// 系统0点时刻与帧时间戳没有任何关系，是指系统认为的第一帧应该准备好的系统时刻点...
	/////////////////////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：越快设置第一帧时间戳，播放延时越小，至于能否播放，不用管，这里只管设定启动时间戳...
	// 获取第一帧的PTS时间戳 => 做为启动时间戳，注意不是系统0点时刻...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if( m_start_pts_ms < 0 ) {
		m_start_pts_ms = inSendTime;
		blog(LOG_INFO, "%s StartPTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}
	// 如果当前帧的时间戳比第一帧的时间戳还要小，不要扔掉，设置成启动时间戳就可以了...
	if( inSendTime < m_start_pts_ms ) {
		blog(LOG_INFO, "%s Error => SendTime: %lu, StartPTS: %I64d", TM_RECV_NAME, inSendTime, m_start_pts_ms);
		inSendTime = m_start_pts_ms;
	}
	// 计算当前帧的时间戳 => 时间戳必须做修正，否则会混乱...
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts_ms;

	// 注意：寻找第一个视频关键帧的时候，音频帧不要丢弃...
	// 如果有视频，视频第一帧必须是视频关键帧，不丢弃的话解码会失败...
	if((inTypeTag == PT_TAG_VIDEO) && (m_lpVideoThread != NULL) && (!m_bFindFirstVKey)) {
		// 如果当前视频帧，不是关键帧，直接丢弃 => 显示计算后的延时毫秒数...
		if( !bIsKeyFrame ) {
			//blog(LOG_INFO, "%s Discard for First Video KeyFrame => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
			m_lpViewRender->doUpdateNotice(QString(QTStr("Render.Window.DropVideoFrame")).arg(nCalcPTS));
			return;
		}
		// 设置已经找到第一个视频关键帧标志...
		m_bFindFirstVKey = true;
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.FindFirstVKey"), true);
		blog(LOG_INFO, "%s Find First Video KeyFrame OK => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}

	///////////////////////////////////////////////////////////////////////////
	// 延时实验：在0~2000毫秒之间来回跳动，跳动间隔为1毫秒...
	///////////////////////////////////////////////////////////////////////////
	/*static bool bForwardFlag = true;
	if( bForwardFlag ) {
		m_zero_delay_ms -= 2;
		bForwardFlag = ((m_zero_delay_ms == 0) ? false : true);
	} else {
		m_zero_delay_ms += 2;
		bForwardFlag = ((m_zero_delay_ms == 2000) ? true : false);
	}*/
	//blog(LOG_INFO, "[Teacher-Looker] Zero Delay => %I64d ms", m_zero_delay_ms);

	///////////////////////////////////////////////////////////////////////////
	// 注意：延时模拟目前已经解决了，后续配合网络实测后再模拟...
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	// 随机丢掉数据帧 => 每隔10秒，丢1秒的音视频数据帧...
	///////////////////////////////////////////////////////////////////////////
	//if( (inFrame.dwSendTime/1000>0) && ((inFrame.dwSendTime/1000)%5==0) ) {
	//	blog(LOG_INFO, "%s [%s] Discard Packet, PTS: %d", TM_RECV_NAME, inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", nCalcPTS);
	//	return;
	//}

	// 如果是音频数据包，并且音频处理对象有效，进行数据投递...
	if( m_lpAudioThread != NULL && inTypeTag == FLV_TAG_TYPE_AUDIO ) {
		m_lpAudioThread->doFillPacket(inData, nCalcPTS, bIsKeyFrame, 0);
	}
	// 如果是视频数据包，并且视频处理对象有效，进行数据投递...
	if( m_lpVideoThread != NULL && inTypeTag == FLV_TAG_TYPE_VIDEO ) {
		m_lpVideoThread->doFillPacket(inData, nCalcPTS, bIsKeyFrame, 0);
	}
	//blog(LOG_INFO, "[%s] RenderOffset: %lu", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", inFrame.dwRenderOffset);
}

/*static bool DoProcSaveJpeg(AVFrame * pSrcFrame, AVPixelFormat inSrcFormat, int64_t inPTS, LPCTSTR lpPath)
{
	char szSavePath[MAX_PATH] = {0};
	sprintf(szSavePath, "%s/%I64d.jpg", lpPath, inPTS);
	/////////////////////////////////////////////////////////////////////////
	// 注意：input->conversion 是需要变换的格式，
	// 因此，应该从 video->info 当中获取原始数据信息...
	// 同时，sws_getContext 需要AVPixelFormat而不是video_format格式...
	/////////////////////////////////////////////////////////////////////////
	// 设置ffmpeg的日志回调函数...
	//av_log_set_level(AV_LOG_VERBOSE);
	//av_log_set_callback(my_av_logoutput);
	// 统一数据源输入格式，找到压缩器需要的像素格式...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = inSrcFormat;
	int nSrcWidth = pSrcFrame->width;
	int nSrcHeight = pSrcFrame->height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);

	// 注意：这里不用libyuv的原因是，使用sws更简单，不用根据不同像素格式调用不同接口...
	// ffmpeg自带的sws_scale转换也是没有问题的，之前有问题是由于sws_getContext的输入源需要格式AVPixelFormat，写成了video_format，造成的格式错位问题...
	// 注意：目的像素格式不能为AV_PIX_FMT_YUVJ420P，会提示警告信息，但并不影响格式转换，因此，还是使用不会警告的AV_PIX_FMT_YUV420P格式...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)pSrcFrame->data, pSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
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
		pOutCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P; //avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, (AVPixelFormat)-1, 1, 0);
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
		FILE * pFile = fopen(szSavePath, "wb");
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
