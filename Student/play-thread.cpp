
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
	// ��ʼ���������������...
	pthread_mutex_init_value(&m_VideoMutex);
	// ��ʼ��COMϵͳ����...
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	// ��ʼ��SDL2.0 => QT�̵߳����Ѿ������� CoInitializeEx()...
	int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
}

CVideoPlay::~CVideoPlay()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ر���Ƶ�������...
	this->doDecoderFree();
	// ����SDL����...
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
	// ����SDL����ʱ�����ع�������...
	if (m_lpViewPlayer != NULL) {
		HWND hWnd = m_lpViewPlayer->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
	}
	// �ͷŵ�֡ͼ��ת���ռ�...
	if (m_img_buffer_ptr != NULL) {
		av_free(m_img_buffer_ptr);
		m_img_buffer_ptr = NULL;
		m_img_buffer_size = 0;
	}
	// �ͷŽ������������...
	pthread_mutex_destroy(&m_VideoMutex);
}

// �ж�SDL�Ƿ���Ҫ�ؽ�...
bool CVideoPlay::IsCanRebuildSDL(int nPicWidth, int nPicHeight)
{
	// ����Ҳಥ���߳�ֹͣ������״̬�仯��SDL���˳��ᵼ�»����ؽ�...
	/*if (m_bUdpRecvThreadStop) {
		// ע�⣺�Ҳ�SDL�����٣��������³�ʼ��SDL���ܳɹ��������������ؽ�SDL...
		int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
		m_lpViewPlayer->GetAndResetRenderFlag();
		m_bUdpRecvThreadStop = false;
		return true;
	}*/
	// ����Ŀ�߷����仯����Ҫ�ؽ�����Ҫ�����ڱ仯�������ظ�����...
	if (m_nSDLTextureWidth != nPicWidth || m_nSDLTextureHeight != nPicHeight) {
		m_lpViewPlayer->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_nSDLTextureWidth == nPicWidth && m_nSDLTextureHeight == nPicHeight);
	// SDL��ض�����Ч����Ҫ�ؽ�����Ҫ�����ڱ仯�������ظ�����...
	if (m_sdlScreen == NULL || m_sdlRenderer == NULL || m_sdlTexture == NULL) {
		m_lpViewPlayer->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_sdlScreen != NULL && m_sdlRenderer != NULL && m_sdlTexture != NULL);
	return m_lpViewPlayer->GetAndResetRenderFlag();
}

void CVideoPlay::doReBuildSDL(int nPicWidth, int nPicHeight)
{
	// ����SDL���� => ���Զ����ع�������...
	if (m_sdlScreen != NULL) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	// ������Ⱦ��...
	if (m_sdlRenderer != NULL) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	// ��������...
	if (m_sdlTexture != NULL) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// �ؽ�SDL��ض���...
	if (m_lpViewPlayer != NULL) {
		// ����SDL����ʱ�����ع������� => ������Windows��API�ӿ�...
		HWND hWnd = m_lpViewPlayer->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
		// ����SDL��Ҫ�Ķ��� => ���ڡ���Ⱦ������...
		m_sdlScreen = SDL_CreateWindowFrom((void*)hWnd);
		m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
		m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, nPicWidth, nPicHeight);
		// ����Texture�ĸ߿��������߿�ı�ʱ����Ϊ�ؽ�Texture��ƾ֤����...
		m_nSDLTextureHeight = nPicHeight;
		m_nSDLTextureWidth = nPicWidth;
	}
}

void CVideoPlay::doDecoderFree()
{
	// �ͷŽ���ṹ��...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// �ͷŽ���������...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
	}
	// �ͷŶ����еĽ���ǰ�����ݿ�...
	GM_AVPacket::iterator itorPack;
	for (itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// �ͷ�û�в�����ϵĽ���������֡...
	GM_AVFrame::iterator itorFrame;
	for (itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
}

bool CVideoPlay::doInitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	///////////////////////////////////////////////////////////////////////
	// ע�⣺���ﶪ����nWidth|nHeight|nFPS��ʹ�ý���֮���ͼƬ�߿����ȷ...
	///////////////////////////////////////////////////////////////////////
	// ����Ѿ���ʼ����ֱ�ӷ���...
	if (m_lpDecCodec != NULL || m_lpDecContext != NULL)
		return false;
	ASSERT(m_lpDecCodec == NULL && m_lpDecContext == NULL);
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_strSPS = inSPS;
	m_strPPS = inPPS;
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// ������Ҫ�Ľ����� => ���ô���������...
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// ����֧�ֵ���ʱģʽ => ûɶ���ã�������Ͼ������...
	m_lpDecContext->flags |= CODEC_FLAG_LOW_DELAY;
	// ����֧�ֲ�����Ƭ�ν��� => ûɶ����...
	if (m_lpDecCodec->capabilities & CODEC_CAP_TRUNCATED) {
		m_lpDecContext->flags |= CODEC_FLAG_TRUNCATED;
	}
	// ���ý����������������� => �������ò�������...
	//m_lpDecContext->refcounted_frames = 1;
	//m_lpDecContext->has_b_frames = 0;
	// �򿪻�ȡ���Ľ�����...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) < 0) {
		blog(LOG_INFO, "%s [Video] avcodec_open2 failed.", TM_RECV_NAME);
		return false;
	}
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);
	// �����߳̿�ʼ��ת...
	this->Start();
	return true;
}

void CVideoPlay::doPushFrame(FMS_FRAME & inFrame, int inCalcPTS)
{
	// �߳������˳��У�ֱ�ӷ���...
	if (this->IsStopRequested())
		return;
	// ��ʱ���洫�ݹ���������֡������Ϣ...
	string & inData = inFrame.strData;
	bool bIsKeyFrame = inFrame.is_keyframe;
	int inOffset = inFrame.dwRenderOffset;
	int inPTS = inCalcPTS;
	// ÿ���ؼ�֡������sps��pps�����Ż�ӿ�...
	string strCurFrame;
	// ����ǹؼ�֡��������д��sps����д��pps...
	if (bIsKeyFrame) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// �޸���Ƶ֡����ʼ�� => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// ���׷��������...
	strCurFrame.append(inData);
	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 1
	// Ŀǰֻ��I֡��P֡��PTS��DTS��һ�µ�...
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// ������ѹ�����ǰ���е��� => ��Ҫ������̻߳���...
	pthread_mutex_lock(&m_VideoMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_VideoMutex);
}

void CVideoPlay::doSleepTo()
{
	// < 0 ������Ϣ���в�����Ϣ��־ => ��ֱ�ӷ���...
	if (!m_bNeedSleep || m_play_next_ns < 0)
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// ����ȵ�ǰʱ��С(�ѹ���)��ֱ�ӷ���...
	if (m_play_next_ns <= cur_time_ns)
		return;
	// ���㳬ǰʱ��Ĳ�ֵ�������Ϣ10����...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

void CVideoPlay::doDecodeFrame()
{
	// û��Ҫ��������ݰ���ֱ�ӷ��������Ϣ������...
	if (m_MapPacket.size() <= 0) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ��ȡһ��AVPacket���н��������������һ��AVPacketһ���ܽ����һ��Picture...
	// �������ݶ�ʧ��B֡��Ͷ��һ��AVPacket��һ���ܷ���Picture����ʱ�������ͻὫ���ݻ�����������ɽ�����ʱ...
	int got_picture = 0, nResult = 0;
	// Ϊ�˿��ٽ��룬ֻҪ�����ݾͽ��룬ͬʱ�����������̻߳���...
	pthread_mutex_lock(&m_VideoMutex);
	GM_AVPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_VideoMutex);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ֻ�� I֡ �� P֡��û��B֡��Ҫ�ý��������ٶ�������������ݣ�ͨ������has_b_frames�����
	// �����ĵ� => https://bbs.csdn.net/topics/390692774
	//////////////////////////////////////////////////////////////////////////////////////////////////
	m_lpDecContext->has_b_frames = 0;
	// ������ѹ������֡������������н������...
	nResult = avcodec_decode_video2(m_lpDecContext, m_lpDecFrame, &got_picture, &thePacket);
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺Ŀǰֻ�� I֡ �� P֡  => ����ʹ��ȫ��AVFrame���ǳ���Ҫ�����ṩ����AVPacket�Ľ���֧��...
	// ����ʧ�ܻ�û�еõ�����ͼ�� => ����Ҫ����AVPacket�����ݲ��ܽ����ͼ��...
	// �ǳ��ؼ��Ĳ��� => m_lpDFrame ǧ�����ͷţ�������AVPacket���ܽ����ͼ��...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if (nResult < 0 || !got_picture) {
		// ��ӡ����ʧ����Ϣ����ʾ��֡�ĸ���...
		//blog(LOG_INFO, "%s [Video] %s Error => decode_frame failed, BFrame: %d, PTS: %I64d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, (m_lpRenderWnd != NULL) ? "net" : "loc", m_lpDecoder->has_b_frames, thePacket.pts + inStartPtsMS, nResult, thePacket.size);
		// ����ǳ��ؼ������߽�������Ҫ���滵֡(B֡)��һ���н������ֱ���ӵ�������ǵ���ʱ����ģʽ...
		m_lpDecContext->has_b_frames = 0;
		// ��������ʧ�ܵ�����֡...
		av_free_packet(&thePacket);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ʹ��AVFrame����������Чʱ�����ʹ��AVPacket��ʱ���Ҳ��һ����...
	// ��Ϊ�������˵���ʱ��ģʽ����֡���ӵ��ˣ��������ڲ�û�л������ݣ��������ʱ�����λ����...
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = m_lpDecFrame->best_effort_timestamp;
	// ���¿�¡AVFrame���Զ�����ռ䣬��ʱ������...
	AVFrame * lpNewFrame = av_frame_clone(m_lpDecFrame);
	m_MapFrame.insert(pair<int64_t, AVFrame*>(frame_pts_ms, lpNewFrame));
	//DoProcSaveJpeg(m_lpDFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// ������Ҫ�ͷ�AVPacket�Ļ���...
	av_free_packet(&thePacket);
	// �޸���Ϣ״̬ => �Ѿ��н��룬������Ϣ...
	m_bNeedSleep = false;
}

void CVideoPlay::doDisplayVideo()
{
	/////////////////////////////////////////////////////////////////
	// ע�⣺���������AVFrame��ͬһ���߳�˳��ִ�У����軥��...
	/////////////////////////////////////////////////////////////////
	// ���û���ѽ�������֡��ֱ�ӷ�����Ϣ��������...
	if (m_MapFrame.size() <= 0) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ���㵱ǰʱ�̵���ϵͳ0��λ�õ�ʱ��� => ת���ɺ���...
	int64_t sys_cur_ms = (os_gettime_ns() - m_sys_zero_ns) / 1000000;
	// ȡ����һ���ѽ�������֡ => ʱ����С������֡...
	GM_AVFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame = itorItem->second;
	int64_t   frame_pts_ms = itorItem->first;
	// ��ǰ֡����ʾʱ�仹û�е� => ֱ����Ϣ��ֵ...
	if (frame_pts_ms > sys_cur_ms) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms) * 1000000;
		//blog(LOG_INFO, "%s [Video] Advance => PTS: %I64d, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, frame_pts_ms - sys_cur_ms, m_MapPacket.size(), m_MapFrame.size());
		return;
	}
	// ��ӡ���ڲ��ŵĽ������Ƶ����...
	//blog(LOG_INFO, "%s [Video] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), m_MapFrame.size());
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��Ƶ��ʱ֡�����֡�������ܶ��������������ʾ����Ƶ�����ٶ���ԽϿ죬����ʱ��������ˣ�����ɲ������⡣
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ׼����Ҫת���ĸ�ʽ��Ϣ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecContext->pix_fmt;
	// �����ĸ߿�������Ԥ��ĸ߿�һ��...
	int nSrcWidth = m_lpDecContext->width;
	int nSrcHeight = m_lpDecContext->height;
	int nDstWidth = m_lpDecContext->width;   // û��ʹ��Ԥ��ֵm_nDstWidth��������rtsp��ȡ�߿�������;
	int nDstHeight = m_lpDecContext->height; // û��ʹ��Ԥ��ֵm_nDstHeight��������rtsp��ȡ�߿�������;
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ĸ߿�������Ԥ��߿�һ��...
	// ע�⣺����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	// ע�⣺���밴�ս�����ͼ��߿����ת����Ԥ�ȷ���ת���ռ䣬�������ش����ͷ���ʱ�ռ�...
	// ע�⣺��ǰʹ��Ԥ��ֵ������Ҫ�ȴ���SDL�������ڸĽ�Ϊ���Ժ󴴽�SDL����Ԥ��ĸ߿��û�����ˣ�ֻ��Ҫ������ʵ�ʸ߿�Ϳ�����...
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// �����ʽת����Ҫ�Ļ���ռ䣬���Ի���ռ�������·���...
	int dst_buffer_size = avpicture_get_size(nDestFormat, nDstWidth, nDstHeight);
	if (dst_buffer_size > m_img_buffer_size) {
		// ɾ��֮ǰ����ĵ�֡���ռ�...
		if (m_img_buffer_ptr != NULL) {
			av_free(m_img_buffer_ptr);
			m_img_buffer_ptr = NULL;
		}
		// ���·������µĵ�֡���ռ�...
		m_img_buffer_size = dst_buffer_size;
		m_img_buffer_ptr = (uint8_t *)av_malloc(m_img_buffer_size);
	}
	// �����µ�Ŀ��֡����...
	AVPicture pDestFrame = { 0 };
	avpicture_fill(&pDestFrame, m_img_buffer_ptr, nDestFormat, nDstWidth, nDstHeight);
	// ����ffmpeg�ĸ�ʽת���ӿں���������ֻ����ͼ���ʽ��ת��������ͼ��Ĵ�С��������...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nDstWidth, nDstHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame.data, pDestFrame.linesize);
	sws_freeContext(img_convert_ctx);
	//////////////////////////////////////////////////////////////////////////////////
	// ʹ��SDL ���л�����ƹ��� => ���ڴ���ȫ��ʱ�����ܻ��ƣ�����D3D������ͻ����...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_lpViewPlayer != NULL && !m_lpViewPlayer->IsChangeScreen()) {
		// �ý����ͼ��߿�ȥ�ж��Ƿ���Ҫ�ؽ�SDL�����Ŵ��ڷ����仯ҲҪ�ؽ�...
		if (this->IsCanRebuildSDL(nSrcWidth, nSrcHeight)) {
			this->doReBuildSDL(nSrcWidth, nSrcHeight);
		}
		// ��ȡ��Ⱦ���ڵľ�������...
		QRect & rcRect = m_lpViewPlayer->GetRenderRect();
		// ע�⣺�����Դ��ת�����ͼ��Ŀ���ǲ��Ŵ���..
		SDL_Rect srcSdlRect = { 0 };
		SDL_Rect dstSdlRect = { 0 };
		srcSdlRect.w = nDstWidth;
		srcSdlRect.h = nDstHeight;
		dstSdlRect.x = rcRect.left();
		dstSdlRect.y = rcRect.top();
		dstSdlRect.w = rcRect.width();
		dstSdlRect.h = rcRect.height();
		// ����Դͷ��Ŀ�Ŀ�߱�������Ϊ�任���߲ο�...
		float srcRatio = srcSdlRect.w * 1.0f / srcSdlRect.h;
		float dstRatio = dstSdlRect.w * 1.0f / dstSdlRect.h;
		// �Ȱѻ�����Ƶ�Texture�ϣ��ٰ�Texture���ŵ����Ŵ������棬Texture�Ĵ�С�ڴ���ʱʹ�õ���Ԥ��߿�...
		int nResult = SDL_UpdateTexture(m_sdlTexture, &srcSdlRect, pDestFrame.data[0], pDestFrame.linesize[0]);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		nResult = SDL_RenderClear(m_sdlRenderer);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// ���Դͷ������Ŀ�ı������ÿ����Ϊ���ߣ������ø߶���Ϊ����...
		if (srcRatio >= dstRatio) {
			// ����Ŀ�Ĵ��ھ��εĸ߶�ֵ(����) => Դ(��)/Դ(��) = Ŀ(��)/Ŀ(��)
			dstSdlRect.h = srcSdlRect.h * dstSdlRect.w / srcSdlRect.w;
			dstSdlRect.y = (rcRect.height() - dstSdlRect.h) / 2;
		} else {
			// ����Ŀ�Ĵ��ھ��εĿ��ֵ(�߲���) => Դ(��)/Դ(��) = Ŀ(��)/Ŀ(��)
			dstSdlRect.w = srcSdlRect.w * dstSdlRect.h / srcSdlRect.h;
			dstSdlRect.x = (rcRect.width() - dstSdlRect.w) / 2;
		}
		// �������ľ���������п�����ʾ����Ⱦ���ڵ���...
		nResult = SDL_RenderCopy(m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect);
		if (nResult < 0) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// ��ʽ������Ⱦ���ڣ����Ƴ�ͼ����...
		SDL_RenderPresent(m_sdlRenderer);
	}
	// �ͷŲ�ɾ���Ѿ�ʹ�����ԭʼ���ݿ�...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
	// �޸���Ϣ״̬ => �Ѿ��в��ţ�������Ϣ...
	m_bNeedSleep = false;
}

void CVideoPlay::doPushPacket(AVPacket & inPacket)
{
	// ע�⣺���������DTS���򣬾����˽�����Ⱥ�˳��...
	// ע�⣺����ʹ��multimap������ר�Ŵ�����ͬʱ���...
	m_MapPacket.insert(pair<int64_t, AVPacket>(inPacket.dts, inPacket));
}

void CVideoPlay::Entry()
{
	while (!this->IsStopRequested()) {
		// ������Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
		m_bNeedSleep = true;
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplayVideo();
		// ����sleep�ȴ�...
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
	// ����Ĭ�ϲ���ֵ...
	m_in_rate_index = 0;
	m_in_sample_rate = 0;
	m_in_channel_num = 0;
	m_in_sample_fmt = AV_SAMPLE_FMT_FLTP;
	// ����Ĭ��������������������...
	m_out_channel_num = 0;
	m_out_sample_rate = 0;
	m_out_sample_fmt = AV_SAMPLE_FMT_FLT;
	// ��ʼ��PCM���ݻ��ζ���...
	circlebuf_init(&m_circle);
	// ��ʼ���������������...
	pthread_mutex_init_value(&m_AudioMutex);
	// ��ʼ��COMϵͳ����...
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

CAudioPlay::~CAudioPlay()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ر���Ƶ�������...
	this->doDecoderFree();
	// �ر���Ƶ�����豸...
	this->doMonitorFree();
	// �ͷ���Ƶ���ζ���...
	circlebuf_free(&m_circle);
	// �ͷŽ������������...
	pthread_mutex_destroy(&m_AudioMutex);
}

void CAudioPlay::doDecoderFree()
{
	// �ͷŽ���ṹ��...
	if (m_lpDecFrame != NULL) {
		av_frame_free(&m_lpDecFrame);
		m_lpDecFrame = NULL;
	}
	// �ͷŽ���������...
	if (m_lpDecContext != NULL) {
		avcodec_close(m_lpDecContext);
		av_free(m_lpDecContext);
	}
	// �ͷŶ����еĽ���ǰ�����ݿ�...
	GM_AVPacket::iterator itorPack;
	for (itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// �ͷ���������Ƶת����...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
}

void CAudioPlay::doMonitorFree()
{
	// ֹͣWSAPI��Ƶ��Դ...
	ULONG uRef = 0;
	HRESULT hr = S_OK;
	if (m_client != NULL) {
		hr = m_client->Stop();
	}
	// �ͷ�WSAPI���ö���...
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
	// ����Ѿ���ʼ����ֱ�ӷ���...
	if (m_lpDecCodec != NULL || m_lpDecContext != NULL)
		return false;
	ASSERT(m_lpDecCodec == NULL && m_lpDecContext == NULL);
	// ��ʼ��WSAPI����Ƶ���г�ʼ������...
	IMMDeviceEnumerator *immde = NULL;
	WAVEFORMATEX *wfex = NULL;
	bool success = false;
	HRESULT hr = S_OK;
	UINT32 frames = 0;
	do {
		// �����豸ö�ٶ���...
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
		// ����WASAPI����Ĳ����ʺ�������...
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
	// �ͷŲ���Ҫ���м����...
	if (immde != NULL) {
		immde->Release();
	}
	if (wfex != NULL) {
		CoTaskMemFree(wfex);
	}
	// �����ʼ��ʧ�ܣ�ֱ�ӷ���...
	if (!success) {
		this->doMonitorFree();
		return false;
	}
	// ����������ȡ������...
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
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ������Ҫ�Ľ����������������������...
	m_lpDecCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecContext = avcodec_alloc_context3(m_lpDecCodec);
	// �򿪻�ȡ���Ľ�����...
	if (avcodec_open2(m_lpDecContext, m_lpDecCodec, NULL) != 0) {
		blog(LOG_INFO, "Error => CAudioPlay::doInitAudio() => avcodec_open2");
		return false;
	}
	// �����������������������...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	m_in_sample_fmt = m_lpDecContext->sample_fmt;
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDecFrame = av_frame_alloc();
	ASSERT(m_lpDecFrame != NULL);
	// ���ý�������Ƶ��ʽ...
	resample_info decInfo = { 0 };
	decInfo.samples_per_sec = m_in_sample_rate;
	decInfo.speakers = CStudentApp::convert_speaker_layout(m_in_channel_num);
	decInfo.format = CStudentApp::convert_sample_format(m_in_sample_fmt);
	// ������Ƶ�����������(WASAPI)��Ҫ�ĸ�ʽ...
	m_horn_sample_info.samples_per_sec = m_out_sample_rate;
	m_horn_sample_info.speakers = CStudentApp::convert_speaker_layout(m_out_channel_num);
	m_horn_sample_info.format = CStudentApp::convert_sample_format(m_out_sample_fmt);
	// �����������طŵ��ز������󣬽�������Ƶ����Ҫ����һ���������������ز���...
	m_horn_resampler = audio_resampler_create(&m_horn_sample_info, &decInfo);
	// �����ز�������ʧ�ܣ���ӡ���󷵻�...
	if (m_horn_resampler == NULL) {
		blog(LOG_INFO, "Error => CAudioPlay::doInitAudio() => audio_resampler_create");
		return false;
	}
	// ���������Ƶ�������� => AAC-1024 | MP3-1152
	int in_nb_samples = 1024;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, m_out_sample_rate, m_in_sample_rate, AV_ROUND_UP);
	// �������ÿ֡����ʱ��(����)��ÿ֡�ֽ��� => ֻ��Ҫ����һ�ξ͹���...
	m_out_frame_duration = out_nb_samples * 1000 / m_out_sample_rate;
	m_out_frame_bytes = av_samples_get_buffer_size(NULL, m_out_channel_num, out_nb_samples, m_out_sample_fmt, 1);
	// �����߳�...
	this->Start();
	return true;
}

// ��Ƶ����֡������ӿڣ�Ϊ�˲��������̣߳�ֱ��Ͷ�ݵ����浱��...
void CAudioPlay::doPushFrame(FMS_FRAME & inFrame, int inCalcPTS)
{
	// �߳������˳��У�ֱ�ӷ���...
	if (this->IsStopRequested())
		return;
	// ������Ƶ����֡��ֱ�ӷ���...
	if (inFrame.typeFlvTag != PT_TAG_AUDIO)
		return;
	// ����������Ϊ�գ�ֱ�ӷ���ʧ��...
	if (m_lpDecContext == NULL || m_lpDecCodec == NULL || m_lpDecFrame == NULL)
		return;
	// �����������Ƶת������Ч��ֱ�ӷ���...
	if (m_horn_resampler == NULL)
		return;
	// ����ADTS���ݰ�ͷ...
	string & inData = inFrame.strData;
	uint32_t inPTS = inCalcPTS; //inFrame.dwSendTime;
	uint32_t inOffset = inFrame.dwRenderOffset;
	bool bIsKeyFrame = inFrame.is_keyframe;
	// �ȼ���ADTSͷ���ټ�������֡����...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// ����ADTS֡ͷ...
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

	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = { 0 };
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// �����֡ͷ���ݣ������֡������Ϣ...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	// ������ѹ�����ǰ���е��� => ��Ҫ������̻߳���...
	pthread_mutex_lock(&m_AudioMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_AudioMutex);
}

void CAudioPlay::doPushPacket(AVPacket & inPacket)
{
	// ע�⣺���������DTS���򣬾����˽�����Ⱥ�˳��...
	// ע�⣺����ʹ��multimap������ר�Ŵ�����ͬʱ���...
	m_MapPacket.insert(pair<int64_t, AVPacket>(inPacket.dts, inPacket));
}

void CAudioPlay::doDecodeFrame()
{
	// û��Ҫ��������ݰ���ֱ�ӷ��������Ϣ������...
	if (m_MapPacket.size() <= 0 || m_render == NULL) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ��ȡһ��AVPacket���н��������һ��AVPacket��һ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	// Ϊ�˿��ٽ��룬ֻҪ�����ݾͽ��룬ͬʱ�����������̻߳���...
	pthread_mutex_lock(&m_AudioMutex);
	GM_AVPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_AudioMutex);
	// ע�⣺��������ĸ�ʽ��AV_SAMPLE_FMT_FLTP����Ҫת������������Ҫfloat��ʽ...
	nResult = avcodec_decode_audio4(m_lpDecContext, m_lpDecFrame, &got_picture, &thePacket);
	// ���û�н���������ݰ�����ӡ��Ϣ���ͷ����ݰ�...
	if (nResult < 0 || !got_picture) {
		blog(LOG_INFO, "[Audio] Error => decode_audio failed, PTS: %I64d, DecodeSize: %d, PacketSize: %d", thePacket.pts, nResult, thePacket.size);
		av_free_packet(&thePacket);
		return;
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	int64_t   frame_pts_ms = thePacket.pts;
	// ��ԭʼ��������Ƶ������ʽ��ת������������Ҫ��������ʽ��ת���ɹ����뻷�ζ���...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, m_lpDecFrame->data, m_lpDecFrame->nb_samples)) {
		// �����ʽת��֮����������ݳ��ȣ�����ת��������ݷ�����黺�浱��...
		int cur_data_size = get_audio_size(m_horn_sample_info.format, m_horn_sample_info.speakers, output_frames);
		// ѹ�뻷�ζ��е��У����ÿ���������� => PTS|Size|Data
		circlebuf_push_back(&m_circle, &frame_pts_ms, sizeof(int64_t));
		circlebuf_push_back(&m_circle, &cur_data_size, sizeof(int));
		circlebuf_push_back(&m_circle, output_data[0], cur_data_size);
		//blog(LOG_INFO, "%s [Audio-Decode] CurTime: %I64d ms, PTS: %I64d ms, Frame: %d, Duration:%.2f", TM_RECV_NAME,
		//	os_gettime_ns()/1000000, frame_pts_ms, output_frames, 1000*output_frames/(1.0f*m_horn_sample_info.samples_per_sec));
		// �ۼӻ��ζ�������Ч����֡�ĸ���...
		++m_frame_num;
	}
	// �ͷ�AVPacket���ݰ�...
	av_free_packet(&thePacket);
	// ������Ϣ����Ҫ��������...
	m_bNeedSleep = false;
}

void CAudioPlay::doDisplayAudio()
{
	// ���û���ѽ�������֡��ֱ�ӷ��������Ϣ������...
	if (m_circle.size <= 0 || m_render == NULL) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ���㵱ǰʱ����0��λ�õ�ʱ��� => ת���ɺ���...
	int64_t sys_cur_ms = (os_gettime_ns() - m_sys_zero_ns) / 1000000;
	// ��ȡ��һ���ѽ�������֡ => ʱ����С������֡...
	int64_t frame_pts_ms = 0; int out_buffer_size = 0;
	circlebuf_peek_front(&m_circle, &frame_pts_ms, sizeof(int64_t));
	// ���ܳ�ǰͶ�����ݣ������Ӳ�������ݶѻ�����ɻ����ѹ��������������...
	if (frame_pts_ms > sys_cur_ms) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms) * 1000000;
		return;
	}
	// ��ǰ֡ʱ����ӻ��ζ��е����Ƴ� => ����������֡����...
	circlebuf_pop_front(&m_circle, NULL, sizeof(int64_t));
	// �ӻ��ζ��е��У�ȡ����Ƶ����֡���ݣ���̬����...
	circlebuf_pop_front(&m_circle, &out_buffer_size, sizeof(int));
	// ����Ǿ���״̬��ֱ�Ӷ�����Ƶ���ݣ�ֱ�ӷ���...
	if (m_bIsMute) {
		// ɾ���Ѿ�ʹ�õ���Ƶ���� => �ӻ��ζ������Ƴ�...
		circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
		// ���ٻ��ζ�������Ч����֡�ĸ���...
		--m_frame_num;
		// �Ѿ��в��ţ�������Ϣ...
		m_bNeedSleep = false;
		return;
	}
	// �������ݿ�ĳ���ΪPCM�������ݿռ�...
	if (out_buffer_size > m_strHorn.size()) {
		m_strHorn.resize(out_buffer_size);
	}
	// ��ǰ����֡����һ��С�ڻ�������֡����ռ�...
	ASSERT(out_buffer_size <= m_strHorn.size());
	// �ӻ��ζ��ж�ȡ��ǰ֡���ݣ���Ϊ��˳��ִ�У������ٴ�ʹ�õ�֡�������ռ�...
	circlebuf_peek_front(&m_circle, (void*)m_strHorn.c_str(), out_buffer_size);

	// ���㵱ǰ���ݿ��������Ч��������...
	BYTE * out_buffer_ptr = (BYTE*)m_strHorn.c_str();
	int nPerFrameSize = (m_out_channel_num * sizeof(float));
	uint32_t resample_frames = out_buffer_size / nPerFrameSize;

	// �����������ݵ�ת�� => ������������ķŴ�...
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
	// ��ȡ��ǰ�����ѻ����֡����...
	hr = m_client->GetCurrentPadding(&nCurPadFrame);
	// �������������ѻ���ĺ����� => ����ȡ��...
	msInSndCardBuf = (int)((nCurPadFrame * 1.0f * nPerFrameSize / m_out_frame_bytes) * m_out_frame_duration + 0.5f);
	// ��300������������棬ת����֡����...
	nAllowQueueFrame = (300.0f / m_out_frame_duration * m_out_frame_bytes) / nPerFrameSize;
	// ֻ�е���������С��300����ʱ���Ž�������Ͷ�ݣ�����300���룬ֱ�Ӷ���...
	if (nCurPadFrame <= nAllowQueueFrame) {
		// �õ���Ҫ������������ָ�� => �Ѿ������Ҫ����֡��...
		hr = m_render->GetBuffer(resample_frames, &output);
		if (SUCCEEDED(hr) && output != NULL) {
			// ��������䵽ָ���Ļ��棬��Ͷ�ݵ���������...
			memcpy(output, out_buffer_ptr, out_buffer_size);
			hr = m_render->ReleaseBuffer(resample_frames, 0);
		}
	}
	// ��ӡ���ڲ��ŵĽ������Ƶ����...
	//blog(LOG_INFO, "%s [Audio] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, QueueFrame: %d/%d",
	//	TM_RECV_NAME, frame_pts_ms, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), nCurPadFrame, nAllowQueueFrame);
	// ɾ���Ѿ�ʹ�õ���Ƶ���� => �ӻ��ζ������Ƴ�...
	circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
	// ���ٻ��ζ�������Ч����֡�ĸ���...
	--m_frame_num;
	// �Ѿ��в��ţ�������Ϣ...
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
	// < 0 ������Ϣ���в�����Ϣ��־ => ��ֱ�ӷ���...
	if (!m_bNeedSleep || m_play_next_ns < 0)
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// ����ȵ�ǰʱ��С(�ѹ���)��ֱ�ӷ���...
	if (m_play_next_ns <= cur_time_ns)
		return;
	// ���㳬ǰʱ��Ĳ�ֵ�������Ϣ10����...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	os_sleepto_ns(cur_time_ns + delta_ns);
}

void CAudioPlay::Entry()
{
	while (!this->IsStopRequested()) {
		// ������Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
		m_bNeedSleep = true;
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplayAudio();
		// ����sleep�ȴ�...
		this->doSleepTo();
	}
}