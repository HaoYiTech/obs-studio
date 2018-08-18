
#include "student-app.h"
#include "pull-thread.h"
#include "myRTSPClient.h"
#include "AmfByteStream.h"

//#include "UtilTool.h"
//#include "libmp4v2\RecThread.h"
//#include "librtmp\AmfByteStream.h"

void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterOPTIONS(resultCode, resultString);
	delete[] resultString;
}

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterDESCRIBE(resultCode, resultString);
	delete[] resultString;
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterSETUP(resultCode, resultString);
	delete[] resultString;
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterPLAY(resultCode, resultString);
	delete[] resultString;
}

StreamClientState::StreamClientState()
  : m_iter(NULL),
    m_session(NULL),
	m_subsession(NULL),
	m_streamTimerTask(NULL),
	m_duration(0.0)
{
}

StreamClientState::~StreamClientState()
{
	if( m_iter != NULL ) {
		delete m_iter;
		m_iter = NULL;
	}

	// We also need to delete "session", and unschedule "streamTimerTask" (if set)
	if( m_session != NULL ) {
		UsageEnvironment& env = m_session->envir(); // alias
		env.taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
		Medium::close(m_session);
	}
}

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,	int verbosityLevel,
							char const* applicationName, BOOL bStreamUsingTCP, CDataThread * lpDataThread)
{
	return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, bStreamUsingTCP, lpDataThread);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL, int verbosityLevel, char const* applicationName, BOOL bStreamUsingTCP, CDataThread * lpDataThread)
  : RTSPClient(env, rtspURL, verbosityLevel, applicationName, 0, -1)
  , m_lpDataThread(lpDataThread)
  , m_bUsingTCP(bStreamUsingTCP)
  , m_bHasVideo(false)
  , m_bHasAudio(false)
{
}

ourRTSPClient::~ourRTSPClient()
{
}

void ourRTSPClient::myAfterOPTIONS(int resultCode, char* resultString)
{
	do {
		// 发生错误，打印返回 => 需要转换成UTF8格式...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[OPTIONS] Code = %d, Error = %s", resultCode, CStudentApp::ANSI_UTF8(resultString).c_str());
			break;
		}
		// 成功，发起DESCRIBE请求...
		blog(LOG_INFO, "[OPTIONS] = %s", CStudentApp::ANSI_UTF8(resultString).c_str());
		this->sendDescribeCommand(continueAfterDESCRIBE);
		return;
	}while( 0 );

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpDataThread != NULL ) {
		m_lpDataThread->ResetEventLoop();
	}
	/*// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}

void ourRTSPClient::myAfterDESCRIBE(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;

		// 打印获取的SDP信息...
		blog(LOG_INFO, "[SDP] = %s", CStudentApp::ANSI_UTF8(resultString).c_str());

		// 返回错误，退出...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[DESCRIBE] Code = %d, Error = %s", resultCode, CStudentApp::ANSI_UTF8(resultString).c_str());
			break;
		}
		
		// 用获取的SDP创建会话...
		scs.m_session = MediaSession::createNew(env, CStudentApp::ANSI_UTF8(resultString).c_str());

		// 判断创建会话的结果...
		if( scs.m_session == NULL ) {
			blog(LOG_INFO, "[DESCRIBE] Error = %s", CStudentApp::ANSI_UTF8(env.getResultMsg()).c_str());
			break;
		} else if ( !scs.m_session->hasSubsessions() ) {
			blog(LOG_INFO, "[DESCRIBE] Error = This session has no media subsessions");
			break;
		}
		
		// Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
		// calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
		// (Each 'subsession' will have its own data source.)
		// 发起下一个SETUP握手操作...
		scs.m_iter = new MediaSubsessionIterator(*scs.m_session);
		this->setupNextSubsession();
		return;
	} while (0);
	
	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpDataThread != NULL ) {
		m_lpDataThread->ResetEventLoop();
	}
	/*// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// 这里可以判断，音视频格式是否符合要求...
// 音视频会各自出现一次，然后才进入到 play 状态...
void ourRTSPClient::myAfterSETUP(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// 返回错误，退出...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[%s/%s] Failed to Setup.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			break;
		}

		// 判断视频格式是否正确，必须是 video/H264 ...
		if( strcmp(scs.m_subsession->mediumName(), "video") == 0 ) {
			if( strcmp(scs.m_subsession->codecName(), "H264") != 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => Must be Video/H264.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// 必须有 SPS 和 PPS 的数据包...
			ASSERT( strcmp(scs.m_subsession->codecName(), "H264") == 0 );
			const char * lpszSpro = scs.m_subsession->fmtp_spropparametersets();
			if( lpszSpro == NULL ) {
				blog(LOG_INFO, "[%s/%s] Error => SPS or PPS...", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// 获得第一个 SPS 和 第一个PPS ...
			ASSERT( lpszSpro != NULL );
			string strSPS, strPPS;
			unsigned numSPropRecords = 0;
			SPropRecord * sPropRecords = parseSPropParameterSets(lpszSpro, numSPropRecords);
			for(unsigned i = 0; i < numSPropRecords; ++i) {
				if( i == 0 && strSPS.size() <= 0 ) {
					strSPS.assign((char*)sPropRecords[i].sPropBytes, sPropRecords[i].sPropLength);
				}
				if( i == 1 && strPPS.size() <= 0 ) {
					strPPS.assign((char*)sPropRecords[i].sPropBytes, sPropRecords[i].sPropLength);
				}
			}
			delete[] sPropRecords;
			// 必须同时包含 SPS 和 PPS...
			if( strSPS.size() <= 0 || strPPS.size() <= 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => SPS or PPS...", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( strSPS.size() > 0 && strPPS.size() > 0 );
			blog(LOG_INFO, "== SPS-Size(%d), PPS-Size(%d) ==", strSPS.size(), strPPS.size());
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// 注意：这时的PPS、SPS有可能是错误的，但是数据区不一定能再次获取到PPS、SPS...
			// 因此：还是在这里处理启动流程，后期如果再遇到问题，可以在数据区获取到新的PPS、SPS后再次发起WriteAVC，并直接发送给服务器试试....
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// 通知rtsp线程，格式头已经准备好了...
			if( m_lpDataThread != NULL ) {
				m_lpDataThread->WriteAVCSequenceHeader(strSPS, strPPS);
			}
			/*// 通知录像线程，格式头已经准备好了...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->StoreVideoHeader(strSPS, strPPS);
			}*/
			// 设置视频有效的标志...
			m_bHasVideo = true;
		}

		// 判断音频格式是否正确, 必须是 audio/MPEG4 ...
		if( strcmp(scs.m_subsession->mediumName(), "audio") == 0 ) {
			if( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) != 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => Must be Audio/MPEG4.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				goto _audio_ok;
			}
			ASSERT( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) == 0 );
			// 获取声道数和采样率信息...
			unsigned audio_channels = scs.m_subsession->numChannels();
			unsigned audio_rate = scs.m_subsession->rtpTimestampFrequency();
			if( audio_channels <= 0 || audio_rate <= 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => channel(%d),rate(%d).", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), audio_channels, audio_rate);
				goto _audio_ok;
			}
			// 通知rtsp线程，格式头已经准备好了...
			if( m_lpDataThread != NULL ) {
				m_lpDataThread->WriteAACSequenceHeader(audio_rate, audio_channels);
			}
			/*// 通知录像线程，格式头已经准备好了...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->StoreAudioHeader(audio_rate, audio_channels);
			}*/
			// 设置音频有效的标志...
			m_bHasAudio = true;
		}
_audio_ok:
		// 打印正确信息...
		blog(LOG_INFO, "[%s/%s] Setup OK.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
		if( scs.m_subsession->rtcpIsMuxed() ) {
			blog(LOG_INFO, "[client port] %d ", scs.m_subsession->clientPortNum());
		} else {
			blog(LOG_INFO, "[client ports] %d - %d ", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
		}
		
		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		// 创建新的帧数据处理对象...		
		// perhaps use your own custom "MediaSink" subclass instead
		ASSERT( m_lpDataThread != NULL );
		scs.m_subsession->sink = DummySink::createNew(env, *scs.m_subsession, this->url(), this);

		// 创建失败，打印错误信息...
		if( scs.m_subsession->sink == NULL ) {
			blog(LOG_INFO, "[%s/%s] Error = %s. ", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), CStudentApp::ANSI_UTF8(env.getResultMsg()).c_str());
			break;
		}
		
		blog(LOG_INFO, "[%s/%s] Created a data sink ok. ", scs.m_subsession->mediumName(), scs.m_subsession->codecName());

		// 发起 PLAY 协议...
		scs.m_subsession->miscPtr = this;
		scs.m_subsession->sink->startPlaying(*(scs.m_subsession->readSource()), subsessionAfterPlaying, scs.m_subsession);

		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if( scs.m_subsession->rtcpInstance() != NULL ) {
			scs.m_subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.m_subsession);
		}

		// Set up the next subsession, if any:
		this->setupNextSubsession();
		return;
	} while (0);

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpDataThread != NULL ) {
		m_lpDataThread->ResetEventLoop();
	}
	/*// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// 进行数据帧的处理...
void ourRTSPClient::WriteSample(bool bIsVideo, string & inFrame, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// 构造音视频数据帧...
	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// 设置音视频标志
	theFrame.dwSendTime = inTimeStamp;
	theFrame.dwRenderOffset = inRenderOffset;
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData = inFrame;
	// 推送给rtsp线程，新的数据帧到达...
	if( m_lpDataThread != NULL ) {
		m_lpDataThread->PushFrame(theFrame);
	}
	/*// 通知录像线程，保存一帧数据包...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->StreamWriteRecord(theFrame);
	}*/
}

void ourRTSPClient::myAfterPLAY(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// 返回错误，打印信息...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "Failed to start playing session.");
			break;
		}
		
		// Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
		// using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
		// 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
		// (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
		if( scs.m_duration > 0 ) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.m_duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.m_duration*1000000);
			scs.m_streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
		}
		
		// 一切正常，打印信息，直接返回...
		blog(LOG_INFO, "Started playing session.(for up to %.2f)", scs.m_duration);
		// 正式启动rtmp推送线程...
		if( m_lpDataThread != NULL ) {
			m_lpDataThread->StartPushThread();
		}
		/*// 正式启动录像过程...
		if( m_lpRecThread != NULL ) {
			m_lpRecThread->StreamBeginRecord();
		}*/
		// 处理完毕，直接返回...
		return;
	} while (0);

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpDataThread != NULL ) {
		m_lpDataThread->ResetEventLoop();
	}
	/*// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// 发起下一个协议...
void ourRTSPClient::setupNextSubsession()
{
	// 获取环境变量信息...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;

	// 获取会话对象...
	scs.m_subsession = scs.m_iter->next();
	if( scs.m_subsession != NULL ) {
		if( !scs.m_subsession->initiate() ) {
			// give up on this subsession; go to the next one
			blog(LOG_INFO, "[%s/%s] Error = %s", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), env.getResultMsg());
			this->setupNextSubsession();
		} else {
			blog(LOG_INFO, "[%s/%s] OK", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			if( scs.m_subsession->rtcpIsMuxed() ) {
				blog(LOG_INFO, "[client port] %d ", scs.m_subsession->clientPortNum());
			} else {
				blog(LOG_INFO, "[client ports] %d - %d ", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
			}
			// Continue setting up this subsession, by sending a RTSP "SETUP" command: 
			// REQUEST_STREAMING_OVER_TCP == True...
			// 发起SETUP协议，这里必须用TCP模式，否则有些服务器不支持RTP模式 => 这里需要加一个开关...
			this->sendSetupCommand(*scs.m_subsession, continueAfterSETUP, False, m_bUsingTCP);
		}
		return;
	}
	
	// 这里发起 PLAY 协议过程...
	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	if( scs.m_session->absStartTime() != NULL ) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		this->sendPlayCommand(*scs.m_session, continueAfterPLAY, scs.m_session->absStartTime(), scs.m_session->absEndTime());
	} else {
		scs.m_duration = scs.m_session->playEndTime() - scs.m_session->playStartTime();
		this->sendPlayCommand(*scs.m_session, continueAfterPLAY);
	}
}
//
// 关闭会话过程...
void ourRTSPClient::shutdownStream()
{
	// 获取环境变量...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;
	
	// First, check whether any subsessions have still to be closed:
	if( scs.m_session != NULL ) { 
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.m_session);
		MediaSubsession* subsession = NULL;
		
		while( (subsession = iter.next()) != NULL ) {
			if( subsession->sink != NULL ) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				
				// in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				if( subsession->rtcpInstance() != NULL ) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL);
				}
				someSubsessionsWereActive = True;
			}
		}
		
		// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
		// Don't bother handling the response to the "TEARDOWN".
		if( someSubsessionsWereActive ) {
			this->sendTeardownCommand(*scs.m_session, NULL);
		}
	}
	
	// 关闭这个rtsp连接对象...
	blog(LOG_INFO, "[%s] Closing the stream.", this->url());
	Medium::close(this);

	// Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
}

// Implementation of the other event handlers:
void subsessionAfterPlaying(void* clientData)
{
	MediaSubsession * subsession = (MediaSubsession*)clientData;
	ourRTSPClient * rtspClient = (ourRTSPClient*)(subsession->miscPtr);
	
	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
	subsession->sink = NULL;
	
	// Next, check whether *all* subsessions' streams have now been closed:
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while( (subsession = iter.next()) != NULL ) {
		if( subsession->sink != NULL )
			return; // this subsession is still active
	}
	
	// 让任务循环退出，然后自动停止会话...
	if( rtspClient->m_lpDataThread != NULL ) {
		rtspClient->m_lpDataThread->ResetEventLoop();
	}
	/*// 让录像线程退出...
	if( rtspClient->m_lpRecThread != NULL ) {
		rtspClient->m_lpRecThread->ResetEventLoop();
	}*/
}

void subsessionByeHandler(void* clientData)
{
	MediaSubsession * subsession = (MediaSubsession*)clientData;
	RTSPClient * rtspClient = (RTSPClient*)subsession->miscPtr;
	UsageEnvironment & env = rtspClient->envir(); // alias
	
	blog(LOG_INFO, "Received RTCP BYTE on %s/%s ", subsession->mediumName(), subsession->codecName());
	
	// Now act as if the subsession had closed:
	subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData)
{
	ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
	StreamClientState& scs = rtspClient->m_scs;
	
	scs.m_streamTimerTask = NULL;
	
	// 让任务循环退出，然后自动停止会话...
	if( rtspClient->m_lpDataThread != NULL ) {
		rtspClient->m_lpDataThread->ResetEventLoop();
	}
	/*// 让录像线程退出...
	if( rtspClient->m_lpRecThread != NULL ) {
		rtspClient->m_lpRecThread->ResetEventLoop();
	}*/
}

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1024 * 1024

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, ourRTSPClient * lpRtspClient)
{
	return new DummySink(env, subsession, streamId, lpRtspClient);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, ourRTSPClient * lpRtspClient)
  : MediaSink(env), fSubsession(subsession), fRtspClient(lpRtspClient)
{
	ASSERT( fRtspClient != NULL );
	fStreamId = strDup(streamId);
	fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];

	fSTimeVal.tv_sec = fSTimeVal.tv_usec = 0;
	memset(&m_llTimCountFirst, 0, sizeof(m_llTimCountFirst));
}

DummySink::~DummySink()
{
	if( fReceiveBuffer != NULL ) {
		delete[] fReceiveBuffer;
		fReceiveBuffer = NULL;
	}
	if( fStreamId != NULL ) {
		delete[] fStreamId;
		fStreamId = NULL;
	}
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds)
{
	DummySink* sink = (DummySink*)clientData;
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}
//
// We've just received a frame of data.  (Optionally) print out information about it:
void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds)
{
	// 把第一帧时间戳作为起点时间...
	string			strFrame;
	uint32_t		dwTimeStamp = 0;
	
	// 得到当前时间，时间单位是0.1微妙...
	/*ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	if( m_llTimCountFirst.QuadPart <= 0 ) {
		m_llTimCountFirst.QuadPart = llTimCountCur.QuadPart;
	}
	// 计算时间戳（毫秒)，需要除以10000...
	dwTimeStamp = (llTimCountCur.QuadPart - m_llTimCountFirst.QuadPart)/10000;*/

	// 2018.05.09 - by jackey => 利用硬件时间进行时间戳的计算...
	// DS-2CV3Q21FD-IW 无线小企鹅，必须使用RTSP传递来的时间戳...
	// DS-2DE2402IW-D3/W 无线IPC，必须使用RTSP传递来的时间戳...
	if((fSTimeVal.tv_sec <= 0) && (fSTimeVal.tv_usec <= 0)) {
		fSTimeVal = presentationTime;
	}
	// 计算时间戳(毫秒)，tv_sec是秒，tv_usec是微秒...
	dwTimeStamp = uint32_t((presentationTime.tv_sec - fSTimeVal.tv_sec)*1000.0f + (presentationTime.tv_usec - fSTimeVal.tv_usec)/1000.0f);

	//char szBuf[MAX_PATH] = {0};

	// 获取视频标志，关键帧标志 => 需要判断是否需要向上传递视频数据...
	// H264的 nalu 标志类型1(片段),5(关键帧),7(SPS),8(PPS)...
	if( fRtspClient->m_bHasVideo && strcmp(fSubsession.mediumName(), "video") == 0 ) {
		// 计算关键帧标志...
		BOOL bIsKeyFrame = false;
		BYTE nalType = fReceiveBuffer[0] & 0x1f;
		// 2017.04.10 - by jackey => 如果是SPS或PPS，直接丢弃...
		// 否则会造成HTML5播放器在video标签中无法播放，通过MPlayer发现，写了多余的坏帧，刚好是3个...
		if( nalType > 5 ) {
			// 6|7|8直接丢弃这些头部数据...
			this->continuePlaying();
			return;
		}
		// 设置关键帧标志...
		if( nalType == 5 ) {
			bIsKeyFrame = true;
		}
		// 将起始码0x00000001，替换为nalu的大小...
		char pNalBuf[4] = {0};
		UI32ToBytes(pNalBuf, frameSize);
		// 视频帧需要加入在前面加入帧的长度(4字节)...
		strFrame.assign(pNalBuf, sizeof(pNalBuf));
		strFrame.append((char*)fReceiveBuffer, frameSize);
		//TRACE("[%x][Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", fReceiveBuffer[0], dwTimeStamp, frameSize, bIsKeyFrame);
		//TRACE("[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
		// 向上层传递视频数据帧...
		fRtspClient->WriteSample(true, strFrame, dwTimeStamp, 0, bIsKeyFrame);
	}
	// 获取音频标志，关键帧标志 => 需要判断是否需要向上传递音频数据...
	if( fRtspClient->m_bHasAudio && strcmp(fSubsession.mediumName(), "audio") == 0 ) {
		strFrame.assign((char*)fReceiveBuffer, frameSize);
		//TRACE("[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, true);
		//sprintf(szBuf, "[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
		// 向上层传递音频数据帧 => 必须在这里单独处理，因为有可能不要音频...
		fRtspClient->WriteSample(false, strFrame, dwTimeStamp, 0, true);
	}
	// 处理下一帧的数据...
	this->continuePlaying();
}

Boolean DummySink::continuePlaying()
{
	// sanity check (should not happen)
	if( fSource == NULL )
		return False;
	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE, afterGettingFrame, this, onSourceClosure, this);
	return True;
}
