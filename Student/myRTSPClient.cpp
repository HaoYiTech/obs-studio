
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
							char const* applicationName, BOOL bStreamUsingTCP, CRtspThread * lpRtspThread)
{
	return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, bStreamUsingTCP, lpRtspThread);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL, int verbosityLevel, char const* applicationName, BOOL bStreamUsingTCP, CRtspThread * lpRtspThread)
  : RTSPClient(env, rtspURL, verbosityLevel, applicationName, 0, -1)
  , m_lpRtspThread(lpRtspThread)
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
		// �������󣬴�ӡ���� => ��Ҫת����UTF8��ʽ...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[OPTIONS] Code = %d, Error = %s", resultCode, CStudentApp::ANSI_UTF8(resultString).c_str());
			break;
		}
		// �ɹ�������DESCRIBE����...
		blog(LOG_INFO, "[OPTIONS] = %s", CStudentApp::ANSI_UTF8(resultString).c_str());
		this->sendDescribeCommand(continueAfterDESCRIBE);
		return;
	}while( 0 );

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	/*// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}

void ourRTSPClient::myAfterDESCRIBE(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;

		// ��ӡ��ȡ��SDP��Ϣ...
		blog(LOG_INFO, "[SDP] = %s", CStudentApp::ANSI_UTF8(resultString).c_str());

		// ���ش����˳�...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[DESCRIBE] Code = %d, Error = %s", resultCode, CStudentApp::ANSI_UTF8(resultString).c_str());
			break;
		}
		
		// �û�ȡ��SDP�����Ự...
		scs.m_session = MediaSession::createNew(env, CStudentApp::ANSI_UTF8(resultString).c_str());

		// �жϴ����Ự�Ľ��...
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
		// ������һ��SETUP���ֲ���...
		scs.m_iter = new MediaSubsessionIterator(*scs.m_session);
		this->setupNextSubsession();
		return;
	} while (0);
	
	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	/*// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// ��������жϣ�����Ƶ��ʽ�Ƿ����Ҫ��...
// ����Ƶ����Գ���һ�Σ�Ȼ��Ž��뵽 play ״̬...
void ourRTSPClient::myAfterSETUP(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// ���ش����˳�...
		if( resultCode != 0 ) {
			blog(LOG_INFO, "[%s/%s] Failed to Setup.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			break;
		}

		// �ж���Ƶ��ʽ�Ƿ���ȷ�������� video/H264 ...
		if( strcmp(scs.m_subsession->mediumName(), "video") == 0 ) {
			if( strcmp(scs.m_subsession->codecName(), "H264") != 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => Must be Video/H264.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// ������ SPS �� PPS �����ݰ�...
			ASSERT( strcmp(scs.m_subsession->codecName(), "H264") == 0 );
			const char * lpszSpro = scs.m_subsession->fmtp_spropparametersets();
			if( lpszSpro == NULL ) {
				blog(LOG_INFO, "[%s/%s] Error => SPS or PPS...", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// ��õ�һ�� SPS �� ��һ��PPS ...
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
			// ����ͬʱ���� SPS �� PPS...
			if( strSPS.size() <= 0 || strPPS.size() <= 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => SPS or PPS...", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( strSPS.size() > 0 && strPPS.size() > 0 );
			blog(LOG_INFO, "== SPS-Size(%d), PPS-Size(%d) ==", strSPS.size(), strPPS.size());
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// ע�⣺��ʱ��PPS��SPS�п����Ǵ���ģ�������������һ�����ٴλ�ȡ��PPS��SPS...
			// ��ˣ����������ﴦ���������̣�����������������⣬��������������ȡ���µ�PPS��SPS���ٴη���WriteAVC����ֱ�ӷ��͸�����������....
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// ֪ͨrtsp�̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAVCSequenceHeader(strSPS, strPPS);
			}
			/*// ֪ͨ¼���̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->StoreVideoHeader(strSPS, strPPS);
			}*/
			// ������Ƶ��Ч�ı�־...
			m_bHasVideo = true;
		}

		// �ж���Ƶ��ʽ�Ƿ���ȷ, ������ audio/MPEG4 ...
		if( strcmp(scs.m_subsession->mediumName(), "audio") == 0 ) {
			if( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) != 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => Must be Audio/MPEG4.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				goto _audio_ok;
			}
			ASSERT( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) == 0 );
			// ��ȡ�������Ͳ�������Ϣ...
			unsigned audio_channels = scs.m_subsession->numChannels();
			unsigned audio_rate = scs.m_subsession->rtpTimestampFrequency();
			if( audio_channels <= 0 || audio_rate <= 0 ) {
				blog(LOG_INFO, "[%s/%s] Error => channel(%d),rate(%d).", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), audio_channels, audio_rate);
				goto _audio_ok;
			}
			// ֪ͨrtsp�̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAACSequenceHeader(audio_rate, audio_channels);
			}
			/*// ֪ͨ¼���̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->StoreAudioHeader(audio_rate, audio_channels);
			}*/
			// ������Ƶ��Ч�ı�־...
			m_bHasAudio = true;
		}
_audio_ok:
		// ��ӡ��ȷ��Ϣ...
		blog(LOG_INFO, "[%s/%s] Setup OK.", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
		if( scs.m_subsession->rtcpIsMuxed() ) {
			blog(LOG_INFO, "[client port] %d ", scs.m_subsession->clientPortNum());
		} else {
			blog(LOG_INFO, "[client ports] %d - %d ", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
		}
		
		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		// �����µ�֡���ݴ�������...		
		// perhaps use your own custom "MediaSink" subclass instead
		ASSERT( m_lpRtspThread != NULL );
		scs.m_subsession->sink = DummySink::createNew(env, *scs.m_subsession, this->url(), this);

		// ����ʧ�ܣ���ӡ������Ϣ...
		if( scs.m_subsession->sink == NULL ) {
			blog(LOG_INFO, "[%s/%s] Error = %s. ", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), CStudentApp::ANSI_UTF8(env.getResultMsg()).c_str());
			break;
		}
		
		blog(LOG_INFO, "[%s/%s] Created a data sink ok. ", scs.m_subsession->mediumName(), scs.m_subsession->codecName());

		// ���� PLAY Э��...
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

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	/*// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// ��������֡�Ĵ���...
void ourRTSPClient::WriteSample(bool bIsVideo, string & inFrame, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// ��������Ƶ����֡...
	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// ��������Ƶ��־
	theFrame.dwSendTime = inTimeStamp;
	theFrame.dwRenderOffset = inRenderOffset;
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData = inFrame;
	// ���͸�rtsp�̣߳��µ�����֡����...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->PushFrame(theFrame);
	}
	/*// ֪ͨ¼���̣߳�����һ֡���ݰ�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->StreamWriteRecord(theFrame);
	}*/
}

void ourRTSPClient::myAfterPLAY(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// ���ش��󣬴�ӡ��Ϣ...
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
		
		// һ����������ӡ��Ϣ��ֱ�ӷ���...
		blog(LOG_INFO, "Started playing session.(for up to %.2f)", scs.m_duration);
		// ��ʽ����rtmp�����߳�...
		if( m_lpRtspThread != NULL ) {
			m_lpRtspThread->StartPushThread();
		}
		/*// ��ʽ����¼�����...
		if( m_lpRecThread != NULL ) {
			m_lpRecThread->StreamBeginRecord();
		}*/
		// ������ϣ�ֱ�ӷ���...
		return;
	} while (0);

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	/*// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}*/
}
//
// ������һ��Э��...
void ourRTSPClient::setupNextSubsession()
{
	// ��ȡ����������Ϣ...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;

	// ��ȡ�Ự����...
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
			// ����SETUPЭ�飬���������TCPģʽ��������Щ��������֧��RTPģʽ => ������Ҫ��һ������...
			this->sendSetupCommand(*scs.m_subsession, continueAfterSETUP, False, m_bUsingTCP);
		}
		return;
	}
	
	// ���﷢�� PLAY Э�����...
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
// �رջỰ����...
void ourRTSPClient::shutdownStream()
{
	// ��ȡ��������...
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
	
	// �ر����rtsp���Ӷ���...
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
	
	// ������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	/*// ��¼���߳��˳�...
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
	
	// ������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	/*// ��¼���߳��˳�...
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
	// �ѵ�һ֡ʱ�����Ϊ���ʱ��...
	string			strFrame;
	uint32_t		dwTimeStamp = 0;
	
	// �õ���ǰʱ�䣬ʱ�䵥λ��0.1΢��...
	/*ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	if( m_llTimCountFirst.QuadPart <= 0 ) {
		m_llTimCountFirst.QuadPart = llTimCountCur.QuadPart;
	}
	// ����ʱ���������)����Ҫ����10000...
	dwTimeStamp = (llTimCountCur.QuadPart - m_llTimCountFirst.QuadPart)/10000;*/

	// 2018.05.09 - by jackey => ����Ӳ��ʱ�����ʱ����ļ���...
	// DS-2CV3Q21FD-IW ����С��죬����ʹ��RTSP��������ʱ���...
	// DS-2DE2402IW-D3/W ����IPC������ʹ��RTSP��������ʱ���...
	if((fSTimeVal.tv_sec <= 0) && (fSTimeVal.tv_usec <= 0)) {
		fSTimeVal = presentationTime;
	}
	// ����ʱ���(����)��tv_sec���룬tv_usec��΢��...
	dwTimeStamp = uint32_t((presentationTime.tv_sec - fSTimeVal.tv_sec)*1000.0f + (presentationTime.tv_usec - fSTimeVal.tv_usec)/1000.0f);

	//char szBuf[MAX_PATH] = {0};

	// ��ȡ��Ƶ��־���ؼ�֡��־ => ��Ҫ�ж��Ƿ���Ҫ���ϴ�����Ƶ����...
	// H264�� nalu ��־����1(Ƭ��),5(�ؼ�֡),7(SPS),8(PPS)...
	if( fRtspClient->m_bHasVideo && strcmp(fSubsession.mediumName(), "video") == 0 ) {
		// ����ؼ�֡��־...
		BOOL bIsKeyFrame = false;
		BYTE nalType = fReceiveBuffer[0] & 0x1f;
		// 2017.04.10 - by jackey => �����SPS��PPS��ֱ�Ӷ���...
		// ��������HTML5��������video��ǩ���޷����ţ�ͨ��MPlayer���֣�д�˶���Ļ�֡���պ���3��...
		if( nalType > 5 ) {
			// 6|7|8ֱ�Ӷ�����Щͷ������...
			this->continuePlaying();
			return;
		}
		// ���ùؼ�֡��־...
		if( nalType == 5 ) {
			bIsKeyFrame = true;
		}
		// ����ʼ��0x00000001���滻Ϊnalu�Ĵ�С...
		char pNalBuf[4] = {0};
		UI32ToBytes(pNalBuf, frameSize);
		// ��Ƶ֡��Ҫ������ǰ�����֡�ĳ���(4�ֽ�)...
		strFrame.assign(pNalBuf, sizeof(pNalBuf));
		strFrame.append((char*)fReceiveBuffer, frameSize);
		//TRACE("[%x][Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", fReceiveBuffer[0], dwTimeStamp, frameSize, bIsKeyFrame);
		//TRACE("[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
		// ���ϲ㴫����Ƶ����֡...
		fRtspClient->WriteSample(true, strFrame, dwTimeStamp, 0, bIsKeyFrame);
	}
	// ��ȡ��Ƶ��־���ؼ�֡��־ => ��Ҫ�ж��Ƿ���Ҫ���ϴ�����Ƶ����...
	if( fRtspClient->m_bHasAudio && strcmp(fSubsession.mediumName(), "audio") == 0 ) {
		strFrame.assign((char*)fReceiveBuffer, frameSize);
		//TRACE("[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, true);
		//sprintf(szBuf, "[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
		// ���ϲ㴫����Ƶ����֡ => ���������ﵥ����������Ϊ�п��ܲ�Ҫ��Ƶ...
		fRtspClient->WriteSample(false, strFrame, dwTimeStamp, 0, true);
	}
	// ������һ֡������...
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