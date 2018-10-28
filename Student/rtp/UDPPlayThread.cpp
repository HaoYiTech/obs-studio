
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
	// ��ʼ���������������...
	pthread_mutex_init_value(&m_DecoderMutex);
}

CDecoder::~CDecoder()
{
	// �ͷŽ���ṹ��...
	if( m_lpDFrame != NULL ) {
		av_frame_free(&m_lpDFrame);
		m_lpDFrame = NULL;
	}
	// �ͷŽ���������...
	if( m_lpDecoder != NULL ) {
		avcodec_close(m_lpDecoder);
		av_free(m_lpDecoder);
	}
	// �ͷŶ����еĽ���ǰ�����ݿ�...
	GM_MapPacket::iterator itorPack;
	for(itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// �ͷ�û�в�����ϵĽ���������֡...
	GM_MapFrame::iterator itorFrame;
	for(itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
	// �ͷŽ������������...
	pthread_mutex_destroy(&m_DecoderMutex);
}

void CDecoder::doPushPacket(AVPacket & inPacket)
{
	// ע�⣺���������DTS���򣬾����˽�����Ⱥ�˳��...
	// �������ͬDTS������֡�Ѿ����ڣ�ֱ���ͷ�AVPacket������...
	if( m_MapPacket.find(inPacket.dts) != m_MapPacket.end() ) {
		blog(LOG_INFO, "%s Error => SameDTS: %I64d, StreamID: %d", TM_RECV_NAME, inPacket.dts, inPacket.stream_index);
		av_free_packet(&inPacket);
		return;
	}
	// ���û����ͬDTS������֡����������...
	m_MapPacket[inPacket.dts] = inPacket;
}

void CDecoder::doSleepTo()
{
	// < 0 ������Ϣ���в�����Ϣ��־ => ��ֱ�ӷ���...
	if( !m_bNeedSleep || m_play_next_ns < 0 )
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = MAX_SLEEP_MS * 1000000;
	// ����ȵ�ǰʱ��С(�ѹ���)��ֱ�ӷ���...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// ���㳬ǰʱ��Ĳ�ֵ�������Ϣ10����...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns >= timeout_ns) ? timeout_ns : delta_ns);
	// ����ϵͳ���ߺ���������sleep��Ϣ...
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
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// ����SDL����...
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
	// ����SDL����ʱ�����ع�������...
	if( m_lpViewRender != NULL ) {
		HWND hWnd = m_lpViewRender->GetRenderHWnd();
		BOOL bRet = ::ShowWindow(hWnd, SW_SHOW);
	}
	// �ͷŵ�֡ͼ��ת���ռ�...
	if( m_img_buffer_ptr != NULL ) {
		av_free(m_img_buffer_ptr);
		m_img_buffer_ptr = NULL;
		m_img_buffer_size = 0;
	}
}

// �ж�SDL�Ƿ���Ҫ�ؽ�...
bool CVideoThread::IsCanRebuildSDL(int nPicWidth, int nPicHeight)
{
	// ����Ŀ�߷����仯����Ҫ�ؽ�����Ҫ�����ڱ仯�������ظ�����...
	if (m_nSDLTextureWidth != nPicWidth || m_nSDLTextureHeight != nPicHeight) {
		m_lpViewRender->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_nSDLTextureWidth == nPicWidth && m_nSDLTextureHeight == nPicHeight);
	// SDL��ض�����Ч����Ҫ�ؽ�����Ҫ�����ڱ仯�������ظ�����...
	if (m_sdlScreen == NULL || m_sdlRenderer == NULL || m_sdlTexture == NULL) {
		m_lpViewRender->GetAndResetRenderFlag();
		return true;
	}
	ASSERT(m_sdlScreen != NULL && m_sdlRenderer != NULL && m_sdlTexture != NULL);
	return m_lpViewRender->GetAndResetRenderFlag();
}

void CVideoThread::doReBuildSDL(int nPicWidth, int nPicHeight)
{
	// ����SDL���� => ���Զ����ع�������...
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	// ������Ⱦ��...
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	// ��������...
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// �ؽ�SDL��ض���...
	if( m_lpViewRender != NULL ) {
		// ����SDL����ʱ�����ع������� => ������Windows��API�ӿ�...
		HWND hWnd = m_lpViewRender->GetRenderHWnd();
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

BOOL CVideoThread::InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	///////////////////////////////////////////////////////////////////////
	// ע�⣺���ﶪ����nWidth|nHeight|nFPS��ʹ�ý���֮���ͼƬ�߿����ȷ...
	///////////////////////////////////////////////////////////////////////
	// ����Ѿ���ʼ����ֱ�ӷ���...
	if( m_lpCodec != NULL || m_lpDecoder != NULL )
		return false;
	ASSERT( m_lpCodec == NULL && m_lpDecoder == NULL );
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_strSPS = inSPS;
	m_strPPS = inPPS;
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// ������Ҫ�Ľ����� => ���ô���������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// ����֧�ֵ���ʱģʽ => ûɶ���ã�������Ͼ������...
	m_lpDecoder->flags |= CODEC_FLAG_LOW_DELAY;
	// ����֧�ֲ�����Ƭ�ν��� => ûɶ����...
	if( m_lpCodec->capabilities & CODEC_CAP_TRUNCATED ) {
		m_lpDecoder->flags |= CODEC_FLAG_TRUNCATED;
	}
	// ���ý����������������� => �������ò�������...
	//m_lpDecoder->refcounted_frames = 1;
	//m_lpDecoder->has_b_frames = 0;
	// �򿪻�ȡ���Ľ�����...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) < 0 ) {
		blog(LOG_INFO, "%s [Video] avcodec_open2 failed.", TM_RECV_NAME);
		return false;
	}
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDFrame = av_frame_alloc();
	ASSERT( m_lpDFrame != NULL );
	// �����߳̿�ʼ��ת...
	this->Start();
	return true;
}

void CVideoThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ������Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
		m_bNeedSleep = true;
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplaySDL();
		// ����sleep�ȴ�...
		this->doSleepTo();
	}
}

void CVideoThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// �߳������˳��У�ֱ�ӷ���...
	if( this->IsStopRequested() )
		return;
	// ÿ���ؼ�֡������sps��pps�����Ż�ӿ�...
	string strCurFrame;
	// ����ǹؼ�֡��������д��sps����д��pps...
	if( bIsKeyFrame ) {
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
	AVPacket  theNewPacket = {0};
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
	pthread_mutex_lock(&m_DecoderMutex);
	this->doPushPacket(theNewPacket);
	pthread_mutex_unlock(&m_DecoderMutex);
}
//
// ȡ��һ֡��������Ƶ���ȶ�ϵͳʱ�䣬�����ܷ񲥷�...
void CVideoThread::doDisplaySDL()
{
	/////////////////////////////////////////////////////////////////
	// ע�⣺���������AVFrame��ͬһ���߳�˳��ִ�У����軥��...
	/////////////////////////////////////////////////////////////////

	// ���û���ѽ�������֡��ֱ�ӷ�����Ϣ��������...
	if( m_MapFrame.size() <= 0 ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ����Ϊ�˲���ԭʼPTS����ȡ�ĳ�ʼPTSֵ => ֻ�ڴ�ӡ������Ϣʱʹ��...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// ���㵱ǰʱ�̵���ϵͳ0��λ�õ�ʱ��� => ת���ɺ���...
	int64_t sys_cur_ms = (os_gettime_ns() - m_lpPlaySDL->GetSysZeroNS())/1000000;
	// �ۼ���Ϊ�趨����ʱ������ => ���...
	sys_cur_ms -= m_lpPlaySDL->GetZeroDelayMS();
	// ȡ����һ���ѽ�������֡ => ʱ����С������֡...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame = itorItem->second;
	int64_t   frame_pts_ms = itorItem->first;
	// ��ǰ֡����ʾʱ�仹û�е� => ֱ����Ϣ��ֵ...
	if( frame_pts_ms > sys_cur_ms ) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms)*1000000;
		//blog(LOG_INFO, "%s [Video] Advance => PTS: %I64d, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, frame_pts_ms - sys_cur_ms, m_MapPacket.size(), m_MapFrame.size());
		return;
	}
	// ������ת����jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Dst");
	// ��ӡ���ڲ��ŵĽ������Ƶ����...
	//blog(LOG_INFO, "%s [Video] Player => PTS: %I64d ms, Delay: %I64d ms, AVPackSize: %d, AVFrameSize: %d", TM_RECV_NAME, frame_pts_ms + inStartPtsMS, sys_cur_ms - frame_pts_ms, m_MapPacket.size(), m_MapFrame.size());
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��Ƶ��ʱ֡�����֡�������ܶ��������������ʾ����Ƶ�����ٶ���ԽϿ죬����ʱ��������ˣ�����ɲ������⡣
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ׼����Ҫת���ĸ�ʽ��Ϣ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	// �����ĸ߿�������Ԥ��ĸ߿�һ��...
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	int nDstWidth = m_lpDecoder->width;   // û��ʹ��Ԥ��ֵm_nDstWidth��������rtsp��ȡ�߿�������;
	int nDstHeight = m_lpDecoder->height; // û��ʹ��Ԥ��ֵm_nDstHeight��������rtsp��ȡ�߿�������;
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
	AVPicture pDestFrame = {0};
	avpicture_fill(&pDestFrame, m_img_buffer_ptr, nDestFormat, nDstWidth, nDstHeight);
	// ��һ�ַ�������ʱ����ͼ���ʽת���ռ�ķ���...
	//int nDestBufSize = avpicture_get_size(nDestFormat, nDstWidth, nDstHeight);
	//uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	//avpicture_fill(&pDestFrame, pDestOutBuf, nDestFormat, nDstWidth, nDstHeight);
	// ����ffmpeg�ĸ�ʽת���ӿں���...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nDstWidth, nDstHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame.data, pDestFrame.linesize);
	sws_freeContext(img_convert_ctx);
	//////////////////////////////////////////////////////////////////////////////////
	// ʹ��SDL ���л�����ƹ��� => ���ڴ���ȫ��ʱ�����ܻ��ƣ�����D3D������ͻ����...
	//////////////////////////////////////////////////////////////////////////////////
	if (m_lpViewRender != NULL && !m_lpViewRender->IsChangeScreen()) {
		// �ý����ͼ��߿�ȥ�ж��Ƿ���Ҫ�ؽ�SDL�����Ŵ��ڷ����仯ҲҪ�ؽ�...
		if (this->IsCanRebuildSDL(nSrcWidth, nSrcHeight)) {
			this->doReBuildSDL(nSrcWidth, nSrcHeight);
		}
		// ��ȡ��Ⱦ���ڵľ�������...
		QRect & rcRect = m_lpViewRender->GetRenderRect();
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
		int nResult = SDL_UpdateTexture( m_sdlTexture, &srcSdlRect, pDestFrame.data[0], pDestFrame.linesize[0] );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		nResult = SDL_RenderClear( m_sdlRenderer );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
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
		nResult = SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect );
		if( nResult < 0 ) { blog(LOG_INFO, "%s [Video] Error => %s", TM_RECV_NAME, SDL_GetError()); }
		// ��ʽ������Ⱦ���ڣ����Ƴ�ͼ����...
		SDL_RenderPresent( m_sdlRenderer );
	}
	// �ͷ���ʱ��������ݿռ�...
	//av_free(pDestOutBuf);
	// �ͷŲ�ɾ���Ѿ�ʹ�����ԭʼ���ݿ�...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
	// �޸���Ϣ״̬ => �Ѿ��в��ţ�������Ϣ...
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
	// û��Ҫ��������ݰ���ֱ�ӷ��������Ϣ������...
	if( m_MapPacket.size() <= 0 ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ����Ϊ�˲���ԭʼPTS����ȡ�ĳ�ʼPTSֵ => ֻ�ڴ�ӡ������Ϣʱʹ��...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// ��ȡһ��AVPacket���н��������������һ��AVPacketһ���ܽ����һ��Picture...
	// �������ݶ�ʧ��B֡��Ͷ��һ��AVPacket��һ���ܷ���Picture����ʱ�������ͻὫ���ݻ�����������ɽ�����ʱ...
	int got_picture = 0, nResult = 0;
	// Ϊ�˿��ٽ��룬ֻҪ�����ݾͽ��룬ͬʱ�����������̻߳���...
	pthread_mutex_lock(&m_DecoderMutex);
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_DecoderMutex);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ֻ�� I֡ �� P֡��û��B֡��Ҫ�ý��������ٶ�������������ݣ�ͨ������has_b_frames�����
	// �����ĵ� => https://bbs.csdn.net/topics/390692774
	//////////////////////////////////////////////////////////////////////////////////////////////////
	m_lpDecoder->has_b_frames = 0;
	// ������ѹ������֡������������н������...
	nResult = avcodec_decode_video2(m_lpDecoder, m_lpDFrame, &got_picture, &thePacket);
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺Ŀǰֻ�� I֡ �� P֡  => ����ʹ��ȫ��AVFrame���ǳ���Ҫ�����ṩ����AVPacket�Ľ���֧��...
	// ����ʧ�ܻ�û�еõ�����ͼ�� => ����Ҫ����AVPacket�����ݲ��ܽ����ͼ��...
	// �ǳ��ؼ��Ĳ��� => m_lpDFrame ǧ�����ͷţ�������AVPacket���ܽ����ͼ��...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if( nResult < 0 || !got_picture ) {
		// �������ʧ�ܵ����...
#ifdef DEBUG_DECODE
		(m_lpRenderWnd != NULL) ? DoSaveNetFile(m_lpDFrame, true, thePacket) : DoSaveLocFile(m_lpDFrame, true, thePacket);
#endif // DEBUG_DECODE
		// ��ӡ����ʧ����Ϣ����ʾ��֡�ĸ���...
		//blog(LOG_INFO, "%s [Video] %s Error => decode_frame failed, BFrame: %d, PTS: %I64d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, (m_lpRenderWnd != NULL) ? "net" : "loc", m_lpDecoder->has_b_frames, thePacket.pts + inStartPtsMS, nResult, thePacket.size);
		// ����ǳ��ؼ������߽�������Ҫ���滵֡(B֡)��һ���н������ֱ���ӵ�������ǵ���ʱ����ģʽ...
		m_lpDecoder->has_b_frames = 0;
		// ��������ʧ�ܵ�����֡...
		av_free_packet(&thePacket);
		return;
	}
	// ������Խ��������������ݴ���...
#ifdef DEBUG_DECODE
	(m_lpRenderWnd != NULL) ? DoSaveNetFile(m_lpDFrame, false, thePacket) : DoSaveLocFile(m_lpDFrame, false, thePacket);
#endif // DEBUG_DECODE
	// ��ӡ����֮�������֡��Ϣ...
	//blog(LOG_INFO, "%s [Video] Decode => BFrame: %d, PTS: %I64d, pkt_dts: %I64d, pkt_pts: %I64d, Type: %d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME, m_lpDecoder->has_b_frames,
	//		m_lpDFrame->best_effort_timestamp + inStartPtsMS, m_lpDFrame->pkt_dts + inStartPtsMS, m_lpDFrame->pkt_pts + inStartPtsMS,
	//		m_lpDFrame->pict_type, nResult, thePacket.size );
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ʹ��AVFrame����������Чʱ�����ʹ��AVPacket��ʱ���Ҳ��һ����...
	// ��Ϊ�������˵���ʱ��ģʽ����֡���ӵ��ˣ��������ڲ�û�л������ݣ��������ʱ�����λ����...
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = m_lpDFrame->best_effort_timestamp;
	// ���¿�¡AVFrame���Զ�����ռ䣬��ʱ������...
	m_MapFrame[frame_pts_ms] = av_frame_clone(m_lpDFrame);
	//DoProcSaveJpeg(m_lpDFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// ������Ҫ�ͷ�AVPacket�Ļ���...
	av_free_packet(&thePacket);
	// �޸���Ϣ״̬ => �Ѿ��н��룬������Ϣ...
	m_bNeedSleep = false;

	/*// �������������֡��ʱ��� => ������ת��������...
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
	// Ĭ��������������������...
	m_out_channel_num = 1;
	m_out_sample_rate = 8000;
	m_out_frame_bytes = 0;
	m_out_frame_duration = 0;
	// SDL��Ҫ����AV_SAMPLE_FMT_S16��WSAPI��Ҫ����AV_SAMPLE_FMT_FLT...
	m_out_sample_fmt = AV_SAMPLE_FMT_FLT;
	// ��ʼ��PCM���ݻ��ζ���...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE/4);
}

CAudioThread::~CAudioThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ر���Ƶת������...
	if (m_out_convert_ctx != NULL) {
		swr_free(&m_out_convert_ctx);
		m_out_convert_ctx = NULL;
	}
	// �رյ�֡��󻺳���...
	if (m_max_buffer_ptr != NULL) {
		av_free(m_max_buffer_ptr);
		m_max_buffer_ptr = NULL;
		m_max_buffer_size = 0;
	}
	// �ͷ���Ƶ���ζ���...
	circlebuf_free(&m_circle);

	// �ر���Ƶ�����豸...
	this->doMonitorFree();

	/*// �ر���Ƶ�豸...
	SDL_CloseAudio();
	m_nDeviceID = 0;*/
}

#ifdef DEBUG_AEC
static void DoSaveTeacherPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel)
{
	// ע�⣺PCM���ݱ����ö����Ʒ�ʽ���ļ�...
	static char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "F:/MP4/PCM/Teacher_%d_%d.pcm", nAudioRate, nAudioChannel);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// ���ļ��ɹ�����ʼд����ƵPCM��������...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}
#endif // DEBUG_AEC

void CAudioThread::doDecodeFrame()
{
	// û��Ҫ��������ݰ���ֱ�ӷ��������Ϣ������...
	if( m_MapPacket.size() <= 0 || m_render == NULL ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ����Ϊ�˲���ԭʼPTS����ȡ�ĳ�ʼPTSֵ => ֻ�ڴ�ӡ������Ϣʱʹ��...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// ��ȡһ��AVPacket���н��������һ��AVPacket��һ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	// Ϊ�˿��ٽ��룬ֻҪ�����ݾͽ��룬ͬʱ�����������̻߳���...
	pthread_mutex_lock(&m_DecoderMutex);
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket thePacket = itorItem->second;
	m_MapPacket.erase(itorItem);
	pthread_mutex_unlock(&m_DecoderMutex);
	// ע�⣺��������ĸ�ʽ��4bit����Ҫת����16bit������swr_convert
	nResult = avcodec_decode_audio4(m_lpDecoder, m_lpDFrame, &got_picture, &thePacket);
	////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ʹ��ȫ��AVFrame���ǳ���Ҫ�����ṩ����AVPacket�Ľ���֧��...
	// ����ʧ�ܻ�û�еõ�����ͼ�� => ����Ҫ����AVPacket�����ݲ��ܽ����ͼ��...
	// �ǳ��ؼ��Ĳ��� => m_lpDFrame ǧ�����ͷţ�������AVPacket���ܽ����ͼ��...
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
	// ��ӡ����֮�������֡��Ϣ...
	//blog(LOG_INFO, "%s [Audio] Decode => PTS: %I64d, pkt_dts: %I64d, pkt_pts: %I64d, Type: %d, DecodeSize: %d, PacketSize: %d", TM_RECV_NAME,
	//		m_lpDFrame->best_effort_timestamp + inStartPtsMS, m_lpDFrame->pkt_dts + inStartPtsMS, m_lpDFrame->pkt_pts + inStartPtsMS,
	//		m_lpDFrame->pict_type, nResult, thePacket.size );
	////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺������Ȼʹ��AVPacket�Ľ���ʱ������Ϊ����ʱ��...
	// ������һ�����ڽ���ʧ����ɵ������жϣ�Ҳ���������Ƶ���ŵ��ӳ�����...
	////////////////////////////////////////////////////////////////////////////////////////////
	int64_t frame_pts_ms = thePacket.pts;
	// �Խ�������Ƶ���и�ʽת��...
	this->doConvertAudio(frame_pts_ms, m_lpDFrame);
	// ������Ҫ�ͷ�AVPacket�Ļ���...
	av_free_packet(&thePacket);
	// �޸���Ϣ״̬ => �Ѿ��в��ţ�������Ϣ...
	m_bNeedSleep = false;
}

void CAudioThread::doConvertAudio(int64_t in_pts_ms, AVFrame * lpDFrame)
{
	// ����������������� => ת���ɵ�����...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	// �����������Ƶ��ʽ...
	AVSampleFormat in_sample_fmt = m_lpDecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = m_out_sample_fmt;
	// ����������������� => ת����8K������...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// ���������Ƶ�������� => AAC-1024 | MP3-1152
	// ע�⣺�Ѿ���ʼת����swr_get_delay()�������ϴ�ת������������������ˣ�Ҫ׼��������������Ŀռ�...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + lpDFrame->nb_samples;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// ��ȡ��Ƶ���������Ļ�������С => �����µĿռ䣬�������ϴ�ת��������������Ҫ�Ŀռ���б仯...
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	// �������Ҫ�Ŀռ�����˵�ǰ��֡���ռ䣬��Ҫ�ؽ��ռ�...
	if (out_buffer_size > m_max_buffer_size) {
		// ɾ��֮ǰ����ĵ�֡���ռ�...
		if (m_max_buffer_ptr != NULL) {
			av_free(m_max_buffer_ptr);
			m_max_buffer_ptr = NULL;
		}
		// ���·������µĵ�֡���ռ�...
		m_max_buffer_size = out_buffer_size;
		m_max_buffer_ptr = (uint8_t *)av_malloc(out_buffer_size);
	}
	// ������Ƶת���ӿڣ�ת����Ƶ���� => ����ֵ����Ҫ������ʵ�ʻ�ȡ����ת���������� => ʹ�����ռ�ȥ����ת�������������...
	int nResult = swr_convert(m_out_convert_ctx, &m_max_buffer_ptr, m_max_buffer_size, (const uint8_t **)lpDFrame->data, lpDFrame->nb_samples);
	// ת��ʧ�ܣ�ֱ�ӷ��� => û����Ҫ��ȡ��ת������������...
	if (nResult <= 0) return;
	// ��Ҫ��ת�������������ת����ʵ�ʵĻ����С������ܽ���Ϊɶ����SDL����ʱ�����ǻ���������޷�����������⣬��Ϊ������̫������...
	int cur_data_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, nResult, out_sample_fmt, 1);
	// ����ȡ��������뻺����е��� => PTS|Size|Data => �����֡�ռ��г�ȡ����ת������Ч��������...
	circlebuf_push_back(&m_circle, &in_pts_ms, sizeof(int64_t));
	circlebuf_push_back(&m_circle, &cur_data_size, sizeof(int));
	circlebuf_push_back(&m_circle, m_max_buffer_ptr, cur_data_size);
	//blog(LOG_INFO, "[Audio] HornPCM => PTS: %I64d, Size: %d", in_pts_ms, cur_data_size);
	// �ۼӻ��ζ�������Ч����֡�ĸ���...
	++m_frame_num;
#ifdef DEBUG_AEC
	// ע�⣺��ʽͳһ��8K�����ʣ�������...
	DoSaveTeacherPCM(m_max_buffer_ptr, cur_data_size, m_out_sample_rate, m_out_channel_num);
#endif // DEBUG_AEC
}

void CAudioThread::doDisplaySDL()
{
	//////////////////////////////////////////////////////////////////
	// ע�⣺ʹ������Ͷ�ݷ�ʽ��������Ч���ͻص���ɵ���ʱ...
	// ע�⣺���������PCM������ͬһ���߳�˳��ִ�У����軥��...
	/////////////////////////////////////////////////////////////////

	/*////////////////////////////////////////////////////////////////
	// ���ԣ�����������Կ��������綶��֮�󣬺�������һ���Ӿͱ����룬
	// ���ǣ���Ƶ���Ų�������˼��ٲ��ţ�����ʱ�䱻�ۻ�����Ƶ��������
	// �������ݾͷ����ѻ����ۻ���ʱ����������Ƶ�����������������
	// ��ˣ���Ҫ�ҵ��ܹ���ȷ������Ƶ���ŵĹ��ߣ�����ֻ���Ӹ�Ӳ������
	//////////////////////////////////////////////////////////////////
	if( m_nDeviceID > 0 ) {
		blog(LOG_INFO, "%s [Audio] QueueBytes => %d, circle: %lu", TM_RECV_NAME, SDL_GetQueuedAudioSize(m_nDeviceID), m_circle.size);
	}*/

	// ���û���ѽ�������֡��ֱ�ӷ��������Ϣ������...
	if( m_circle.size <= 0 || m_render == NULL ) {
		m_play_next_ns = os_gettime_ns() + MAX_SLEEP_MS * 1000000;
		return;
	}
	// ����Ϊ�˲���ԭʼPTS����ȡ�ĳ�ʼPTSֵ => ֻ�ڴ�ӡ������Ϣʱʹ��...
	int64_t inStartPtsMS = m_lpPlaySDL->GetStartPtsMS();
	// ���㵱ǰʱ����0��λ�õ�ʱ��� => ת���ɺ���...
	int64_t sys_cur_ms = (os_gettime_ns() - m_lpPlaySDL->GetSysZeroNS())/1000000;
	// �ۼ���Ϊ�趨����ʱ������ => ���...
	sys_cur_ms -= m_lpPlaySDL->GetZeroDelayMS();
	// ��ȡ��һ���ѽ�������֡ => ʱ����С������֡...
	int64_t frame_pts_ms = 0; int out_buffer_size = 0;
	circlebuf_peek_front(&m_circle, &frame_pts_ms, sizeof(int64_t));
	// ���ܳ�ǰͶ�����ݣ������Ӳ�������ݶѻ�����ɻ����ѹ��������������...
	if( frame_pts_ms > sys_cur_ms ) {
		m_play_next_ns = os_gettime_ns() + (frame_pts_ms - sys_cur_ms)*1000000;
		return;
	}
	// ��ǰ֡ʱ����ӻ��ζ��е����Ƴ� => ����������֡����...
	circlebuf_pop_front(&m_circle, NULL, sizeof(int64_t));
	// �ӻ��ζ��е��У�ȡ����Ƶ����֡���ݣ���̬����...
	circlebuf_pop_front(&m_circle, &out_buffer_size, sizeof(int));
	// ��ǰ����֡����һ��С�ڻ�������֡����ռ�...
	ASSERT(m_max_buffer_size >= out_buffer_size);
	// �ӻ��ζ��ж�ȡ��ǰ֡���ݣ���Ϊ��˳��ִ�У������ٴ�ʹ�õ�֡�������ռ�...
	circlebuf_peek_front(&m_circle, m_max_buffer_ptr, out_buffer_size);
	
	// ���㵱ǰ���ݿ��������Ч��������...
	int nPerFrameSize = (m_out_channel_num * sizeof(float));
	uint32_t resample_frames = out_buffer_size / nPerFrameSize;
	
	// �����������ݵ�ת�� => ������������ķŴ�...
	float vol = m_lpPlaySDL->GetVolRate();
	if (!close_float(vol, 1.0f, EPSILON)) {
		register float *cur = (float*)m_max_buffer_ptr;
		register float *end = cur + resample_frames * m_out_channel_num;
		while (cur < end) {
			*(cur++) *= vol;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�������Ƶ�����ڲ��Ļ��������ڷ�ֵ���� => CPU����ʱ��DirectSound��ѻ�����...
	// Ͷ������ǰ���Ȳ鿴�����Ŷӵ���Ƶ���� => ���泬��500��������� => �����500�����������֡��(���ֽ���)...
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//int nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	/*int nAllowQueueSize = (int)(500.0f / m_out_frame_duration * m_out_frame_bytes);
	// ����֮��������������Ƶ���ݣ��������ݽ�һ���ѻ�...
	if( nQueueBytes > nAllowQueueSize ) {
		blog(LOG_INFO, "%s [Audio] Clear Audio Buffer, QueueBytes: %d, AVPacket: %d, AVFrame: %d", TM_RECV_NAME, nQueueBytes, m_MapPacket.size(), m_frame_num);
		SDL_ClearQueuedAudio(m_nDeviceID);
	}
	//blog(LOG_INFO, "[Audio] Delay: %d ms", nQueueBytes / 16);
	// ע�⣺ʧ��Ҳ��Ҫ���أ�����ִ�У�Ŀ�����Ƴ�����...
	// ����Ƶ����������֡Ͷ�ݸ���Ƶ�豸...
	if( SDL_QueueAudio(m_nDeviceID, m_max_buffer_ptr, out_buffer_size) < 0 ) {
		blog(LOG_INFO, "%s [Audio] Failed to queue audio: %s", TM_RECV_NAME, SDL_GetError());
	}*/
	
	HRESULT hr = S_OK;
	BYTE * output = NULL;
	UINT32 nCurPadFrame = 0;
	UINT32 nAllowQueueFrame = 0;
	int    msInSndCardBuf = 0;
	// ��ȡ��ǰ�����ѻ����֡����...
	hr = m_client->GetCurrentPadding(&nCurPadFrame);
	// �������������ѻ���ĺ����� => ����ȡ��...
	msInSndCardBuf = (int)((nCurPadFrame * 1.0f * nPerFrameSize / m_out_frame_bytes) * m_out_frame_duration + 0.5f);
	// ��400������������棬ת����֡����...
	nAllowQueueFrame = (300.0f / m_out_frame_duration * m_out_frame_bytes) / nPerFrameSize;
	// ֻ�е���������С��300����ʱ���Ž�������Ͷ�ݣ�����300���룬ֱ�Ӷ���...
	if (nCurPadFrame <= nAllowQueueFrame) {
		// �õ���Ҫ������������ָ�� => �Ѿ������Ҫ����֡��...
		hr = m_render->GetBuffer(resample_frames, &output);
		if (SUCCEEDED(hr) && output != NULL) {
			// ��������䵽ָ���Ļ��棬��Ͷ�ݵ���������...
			memcpy(output, m_max_buffer_ptr, out_buffer_size);
			hr = m_render->ReleaseBuffer(resample_frames, 0);
			// �ѽ�������Ƶ����Ͷ�ݸ���ǰ���ڱ���ȡ��ͨ�����л�������...
			App()->doEchoCancel(m_max_buffer_ptr, out_buffer_size, m_out_sample_rate, m_out_channel_num, msInSndCardBuf);
		}
	} else {
		// ע�⣺�������飬�����򱣳�ԭ�������ή��AECЧ��...
		// �����������300���룬���þ�������ӡ������Ϣ...
		memset(m_max_buffer_ptr, out_buffer_size, 0);
		App()->doEchoCancel(m_max_buffer_ptr, out_buffer_size, m_out_sample_rate, m_out_channel_num, msInSndCardBuf);
		blog(LOG_INFO, "%s [Audio] Delayed, Padding: %u, AVPacket: %d, AVFrame: %d", TM_RECV_NAME, nCurPadFrame, m_MapPacket.size(), m_frame_num);
	}
	// ɾ���Ѿ�ʹ�õ���Ƶ���� => �ӻ��ζ������Ƴ�...
	circlebuf_pop_front(&m_circle, NULL, out_buffer_size);
	// ���ٻ��ζ�������Ч����֡�ĸ���...
	--m_frame_num;
	// �޸���Ϣ״̬ => �Ѿ��в��ţ�������Ϣ...
	m_bNeedSleep = false;
}

BOOL CAudioThread::InitAudio(int nInRateIndex, int nInChannelNum)
{
	// ����Ѿ���ʼ����ֱ�ӷ���...
	if( m_lpCodec != NULL || m_lpDecoder != NULL )
		return false;
	ASSERT( m_lpCodec == NULL && m_lpDecoder == NULL );
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
	// �����������������������...
	m_in_rate_index = nInRateIndex;
	m_in_channel_num = nInChannelNum;
	// ������������ʺ��������...
	//m_out_sample_rate = nOutSampleRate;
	//m_out_channel_num = nOutChannelNum;
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ������Ҫ�Ľ����������������������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// �򿪻�ȡ���Ľ�����...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// ׼��һ��ȫ�ֵĽ���ṹ�� => ��������֡���໥������...
	m_lpDFrame = av_frame_alloc();
	ASSERT(m_lpDFrame != NULL);
	// ����������������� => ת���ɵ�����...
	int in_audio_channel_num = m_in_channel_num;
	int out_audio_channel_num = m_out_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// �����������Ƶ��ʽ...
	AVSampleFormat in_sample_fmt = m_lpDecoder->sample_fmt; // => AV_SAMPLE_FMT_FLTP
	AVSampleFormat out_sample_fmt = m_out_sample_fmt;
	// ����������������� => ת����8K������...
	int in_sample_rate = m_in_sample_rate;
	int out_sample_rate = m_out_sample_rate;
	// ���䲢��ʼ��ת����...
	m_out_convert_ctx = swr_alloc();
	m_out_convert_ctx = swr_alloc_set_opts(m_out_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, in_sample_fmt, in_sample_rate, 0, NULL);
	// ��ʼ��ת����ʧ�ܣ���ӡ��Ϣ�����ش���...
	if (AVERROR(swr_init(m_out_convert_ctx)) < 0) {
		blog(LOG_INFO, "error => swr_init");
		return false;
	}

	// ���������Ƶ�������� => AAC-1024 | MP3-1152
	// ע�⣺��û�п�ʼת����swr_get_delay()����0��out_nb_samples���������������������...
	int in_nb_samples = swr_get_delay(m_out_convert_ctx, in_sample_rate) + 1024;
	int out_nb_samples = av_rescale_rnd(in_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
	// �������ÿ֡����ʱ��(����)��ÿ֡�ֽ��� => ֻ��Ҫ����һ�ξ͹���...
	m_out_frame_duration = out_nb_samples * 1000 / out_sample_rate;
	m_out_frame_bytes = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);

	// ͨ������ͨ�������������ɹ�...
	App()->SetAudioHorn(true);

	/*//SDL_AudioSpec => ����ʹ��ϵͳ�Ƽ����� => ���ûص�ģʽ������Ͷ��...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS;
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = NULL; 
	audioSpec.userdata = NULL;
	audioSpec.silence = 0;
	// ��SDL��Ƶ�豸 => ֻ�ܴ�һ���豸...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		//SDL_Log("[Audio] Failed to open audio: %s", SDL_GetError());
		blog(LOG_INFO, "%s [Audio] Failed to open audio: %s", TM_RECV_NAME, SDL_GetError());
		return false;
	}
	// �򿪵����豸���...
	m_nDeviceID = 1;
	// ��ʼ���� => ʹ��Ĭ�����豸...
	SDL_PauseAudioDevice(m_nDeviceID, 0);
	SDL_ClearQueuedAudio(m_nDeviceID);*/

	// �����߳�...
	this->Start();
	return true;
}

void CAudioThread::doMonitorFree()
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
	// ͨ������ͨ���������ͷ����...
	App()->SetAudioHorn(false);
}

void CAudioThread::Entry()
{
	// ע�⣺����߳����ȼ��������ܽ����Ƶ��ϵͳ��������...
	// �����߳����ȼ� => ��߼�����ֹ�ⲿ����...
	//if( !::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST) ) {
	//	blog(LOG_INFO, "[Audio] SetThreadPriority to THREAD_PRIORITY_HIGHEST failed.");
	//}
	while( !this->IsStopRequested() ) {
		// ������Ϣ��־ => ֻҪ�н���򲥷žͲ�����Ϣ...
		m_bNeedSleep = true;
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplaySDL();
		// ����sleep�ȴ�...
		this->doSleepTo();
	}
}
//
// ��Ҫ����Ƶ֡�������ͷ��Ϣ...
void CAudioThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// �߳������˳��У�ֱ�ӷ���...
	if (this->IsStopRequested())
		return;
	// �ȼ���ADTSͷ���ټ�������֡����...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// ����ADTS֡ͷ...
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

	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = {0};
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
	// ��ʼ��SDL2.0 => QT�̵߳����Ѿ������� CoInitializeEx()...
	int nRet = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
}

CPlaySDL::~CPlaySDL()
{
	// �ͷ�����Ƶ�������...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	// �ͷ�SDL2.0��Դ...
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
	// �����µ���Ƶ����...
	if (m_lpVideoThread != NULL) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	// ������Ƶ�̶߳���...
	m_lpVideoThread = new CVideoThread(this, m_lpViewRender);
	// ��ʼ����Ƶ����ʧ�ܣ�ֱ��ɾ����Ƶ���󣬷���ʧ��...
	if (!m_lpVideoThread->InitVideo(inSPS, inPPS, nWidth, nHeight, nFPS)){
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
		return false;
	}
	// ���سɹ�...
	return true;
}

BOOL CPlaySDL::InitAudio(int nInRateIndex, int nInChannelNum)
{
	// �����µ���Ƶ����...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	// ������Ƶ�̶߳���...
	m_lpAudioThread = new CAudioThread(this);
	// ��ʼ����Ƶ����ʧ�ܣ�ֱ��ɾ����Ƶ���󣬷���ʧ��...
	if (!m_lpAudioThread->InitAudio(nInRateIndex, nInChannelNum)) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
		return false;
	}
	// ���سɹ�...
	return true;
}

void CPlaySDL::PushPacket(int zero_delay_ms, string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime)
{
	// Ϊ�˽��ͻ����ʱ������Ҫ��һ������˥���㷨�����в�����ʱ����...
	// ֱ��ʹ�ü�����Ļ���ʱ���趨��ʱʱ�� => ���������ʱ...
	if( m_zero_delay_ms < 0 ) { m_zero_delay_ms = zero_delay_ms; }
	else { m_zero_delay_ms = (7 * m_zero_delay_ms + zero_delay_ms) / 8; }

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��������������ϵͳ0��ʱ�̣�������֮ǰ�趨0��ʱ��...
	// ϵͳ0��ʱ����֡ʱ���û���κι�ϵ����ָϵͳ��Ϊ�ĵ�һ֡Ӧ��׼���õ�ϵͳʱ�̵�...
	/////////////////////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺Խ�����õ�һ֡ʱ�����������ʱԽС�������ܷ񲥷ţ����ùܣ�����ֻ���趨����ʱ���...
	// ��ȡ��һ֡��PTSʱ��� => ��Ϊ����ʱ�����ע�ⲻ��ϵͳ0��ʱ��...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if( m_start_pts_ms < 0 ) {
		m_start_pts_ms = inSendTime;
		blog(LOG_INFO, "%s StartPTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}
	// �����ǰ֡��ʱ����ȵ�һ֡��ʱ�����ҪС����Ҫ�ӵ������ó�����ʱ����Ϳ�����...
	if( inSendTime < m_start_pts_ms ) {
		blog(LOG_INFO, "%s Error => SendTime: %lu, StartPTS: %I64d", TM_RECV_NAME, inSendTime, m_start_pts_ms);
		inSendTime = m_start_pts_ms;
	}
	// ���㵱ǰ֡��ʱ��� => ʱ�����������������������...
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts_ms;

	// ע�⣺Ѱ�ҵ�һ����Ƶ�ؼ�֡��ʱ����Ƶ֡��Ҫ����...
	// �������Ƶ����Ƶ��һ֡��������Ƶ�ؼ�֡���������Ļ������ʧ��...
	if((inTypeTag == PT_TAG_VIDEO) && (m_lpVideoThread != NULL) && (!m_bFindFirstVKey)) {
		// �����ǰ��Ƶ֡�����ǹؼ�֡��ֱ�Ӷ��� => ��ʾ��������ʱ������...
		if( !bIsKeyFrame ) {
			//blog(LOG_INFO, "%s Discard for First Video KeyFrame => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
			m_lpViewRender->doUpdateNotice(QString(QTStr("Render.Window.DropVideoFrame")).arg(nCalcPTS));
			return;
		}
		// �����Ѿ��ҵ���һ����Ƶ�ؼ�֡��־...
		m_bFindFirstVKey = true;
		m_lpViewRender->doUpdateNotice(QTStr("Render.Window.FindFirstVKey"), true);
		blog(LOG_INFO, "%s Find First Video KeyFrame OK => PTS: %lu, Type: %d", TM_RECV_NAME, inSendTime, inTypeTag);
	}

	///////////////////////////////////////////////////////////////////////////
	// ��ʱʵ�飺��0~2000����֮�������������������Ϊ1����...
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
	// ע�⣺��ʱģ��Ŀǰ�Ѿ�����ˣ������������ʵ�����ģ��...
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	// �����������֡ => ÿ��10�룬��1�������Ƶ����֡...
	///////////////////////////////////////////////////////////////////////////
	//if( (inFrame.dwSendTime/1000>0) && ((inFrame.dwSendTime/1000)%5==0) ) {
	//	blog(LOG_INFO, "%s [%s] Discard Packet, PTS: %d", TM_RECV_NAME, inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", nCalcPTS);
	//	return;
	//}

	// �������Ƶ���ݰ���������Ƶ���������Ч����������Ͷ��...
	if( m_lpAudioThread != NULL && inTypeTag == FLV_TAG_TYPE_AUDIO ) {
		m_lpAudioThread->doFillPacket(inData, nCalcPTS, bIsKeyFrame, 0);
	}
	// �������Ƶ���ݰ���������Ƶ���������Ч����������Ͷ��...
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
	// ע�⣺input->conversion ����Ҫ�任�ĸ�ʽ��
	// ��ˣ�Ӧ�ô� video->info ���л�ȡԭʼ������Ϣ...
	// ͬʱ��sws_getContext ��ҪAVPixelFormat������video_format��ʽ...
	/////////////////////////////////////////////////////////////////////////
	// ����ffmpeg����־�ص�����...
	//av_log_set_level(AV_LOG_VERBOSE);
	//av_log_set_callback(my_av_logoutput);
	// ͳһ����Դ�����ʽ���ҵ�ѹ������Ҫ�����ظ�ʽ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = inSrcFormat;
	int nSrcWidth = pSrcFrame->width;
	int nSrcHeight = pSrcFrame->height;
	// ����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);

	// ע�⣺���ﲻ��libyuv��ԭ���ǣ�ʹ��sws���򵥣����ø��ݲ�ͬ���ظ�ʽ���ò�ͬ�ӿ�...
	// ffmpeg�Դ���sws_scaleת��Ҳ��û������ģ�֮ǰ������������sws_getContext������Դ��Ҫ��ʽAVPixelFormat��д����video_format����ɵĸ�ʽ��λ����...
	// ע�⣺Ŀ�����ظ�ʽ����ΪAV_PIX_FMT_YUVJ420P������ʾ������Ϣ��������Ӱ���ʽת������ˣ�����ʹ�ò��ᾯ���AV_PIX_FMT_YUV420P��ʽ...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)pSrcFrame->data, pSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);

	// ����ת���������֡����...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;

	// ��ת����� YUV ���ݴ��̳� jpg �ļ�...
	AVCodecContext * pOutCodecCtx = NULL;
	bool bRetSave = false;
	do {
		// Ԥ�Ȳ���jpegѹ������Ҫ���������ݸ�ʽ...
		AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
		AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
		if (pOutAVCodec == NULL)
			break;
		// ����ffmpegѹ�����ĳ�������...
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// ׼�����ݽṹ��Ҫ�Ĳ���...
		pOutCodecCtx->bit_rate = 200000;
		pOutCodecCtx->width = nSrcWidth;
		pOutCodecCtx->height = nSrcHeight;
		// ע�⣺û��ʹ�����䷽ʽ�����������ʽ�п��ܲ���YUVJ420P�����ѹ������������Ϊ���ݵ������Ѿ��̶���YUV420P...
		// ע�⣺����������YUV420P��ʽ��ѹ����������YUVJ420P��ʽ...
		pOutCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P; //avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, (AVPixelFormat)-1, 1, 0);
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		pOutCodecCtx->time_base.num = 1;
		pOutCodecCtx->time_base.den = 25;
		// �� ffmpeg ѹ����...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// ����ͼ��������Ĭ����0.5���޸�Ϊ0.8(ͼƬ̫��,0.5�ոպ�)...
		pOutCodecCtx->qcompress = 0.5f;
		// ׼�����ջ��棬��ʼѹ��jpg����...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// �����µ�ѹ������...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pDestFrame, &got_pic);
		// ����ʧ�ܻ�û�еõ�����ͼ�񣬼�������...
		if (nResult < 0 || !got_pic)
			break;
		// ��jpg�ļ����...
		FILE * pFile = fopen(szSavePath, "wb");
		// ��jpgʧ�ܣ�ע���ͷ���Դ...
		if (pFile == NULL) {
			av_packet_unref(&pkt);
			break;
		}
		// ���浽���̣����ͷ���Դ...
		fwrite(pkt.data, 1, pkt.size, pFile);
		av_packet_unref(&pkt);
		// �ͷ��ļ���������سɹ�...
		fclose(pFile); pFile = NULL;
		bRetSave = true;
	} while (false);
	// �����м�����Ķ���...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}

	// �ͷ���ʱ��������ݿռ�...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);

	return bRetSave;
}*/
