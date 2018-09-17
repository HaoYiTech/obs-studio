#include "enum-wasapi.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>
#include <util/windows/WinHandle.hpp>
#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>

#include <util/circlebuf.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include "media-io/audio-resampler.h"
#include "media-io/audio-io.h"

using namespace std;

#define OPT_DEVICE_ID         "device_id"
#define OPT_USE_DEVICE_TIMING "use_device_timing"

// Fix inconsistent defs of speaker_surround between avutil & wasapi
#define KSAUDIO_SPEAKER_2POINT1 (KSAUDIO_SPEAKER_STEREO|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_4POINT1 (KSAUDIO_SPEAKER_QUAD|SPEAKER_LOW_FREQUENCY)

class WASAPISource {
	ComPtr<IMMDevice>           device;
	ComPtr<IAudioClient>        client;
	ComPtr<IAudioCaptureClient> capture;
	ComPtr<IAudioRenderClient>  render;

	obs_source_t                *source;
	string                      device_id;
	string                      device_name;
	bool                        isInputDevice;
	bool                        useDeviceTiming = false;
	bool                        isDefaultDevice = false;

	bool                        reconnecting = false;
	bool                        previouslyFailed = false;
	WinHandle                   reconnectThread;

	bool                        active = false;
	WinHandle                   captureThread;

	WinHandle                   stopSignal;
	WinHandle                   receiveSignal;

	int                         m_nSpeexNN;         // ÿ�δ���������
	int                         m_nSpeexTAIL;       // β����������
	short                 *     m_lpMicBufNN;       // ��˷�ԭʼ����
	short                 *     m_lpHornBufNN;      // ������ԭʼ����
	short                 *     m_lpEchoBufNN;      // ��������������
	circlebuf                   m_circle_mic;		          // PCM���ζ��� => ֻ�����˷�ת�������Ƶ����
	circlebuf                   m_circle_horn;                // PCM���ζ��� => ֻ���������ת�������Ƶ����
	resample_info               m_mic_sample_info;            // ��˷�ԭʼ��Ƶ������ʽ...
	resample_info               m_horn_sample_info;           // ������ԭʼ��Ƶ������ʽ...
	resample_info               m_echo_sample_info;           // ����������Ҫ����Ƶ������ʽ...
	audio_resampler_t     *     m_mic_resampler = nullptr;    // ��˷�ԭʼ��������ת���ɻ�������������ʽ...
	audio_resampler_t     *     m_horn_resampler = nullptr;   // ������ԭʼ��������ת���ɻ�������������ʽ...
	pthread_mutex_t             m_SpeexMutex;       // ��������������
	SpeexEchoState_       *     m_lpSpeexEcho;      // ������������
	SpeexPreprocessState_ *     m_lpSpeexDen;       // ������������
	int                         m_horn_delay_ms;    // �������趨��Ĭ�ϲ����ӳ�ʱ��(����)�������ڲ�����ĺ�����
	int                         m_out_channel_num;  // ���������
	int                         m_out_sample_rate;  // ���������

	//speaker_layout            speakers;
	//audio_format              format;
	//uint32_t                  sampleRate;

	static DWORD WINAPI ReconnectThread(LPVOID param);
	static DWORD WINAPI CaptureThread(LPVOID param);

	void doEchoCancel(UINT64 ts);
	bool ProcessCaptureData();

	inline void Start();
	inline void Stop();

	void Reconnect();

	bool InitDevice(IMMDeviceEnumerator *enumerator);
	void InitClient();
	void InitRender();
	void InitFormat(WAVEFORMATEX *wfex);
	void InitCapture();
	void Initialize();

	void InitSpeexAEC();
	void UnInitSpeexAEC();

	bool TryInitialize();

	void UpdateSettings(obs_data_t *settings);
public:
	WASAPISource(obs_data_t *settings, obs_source_t *source_, bool input);
	inline ~WASAPISource();

	void Update(obs_data_t *settings);

	obs_audio_data * AudioEchoCancelFilter(obs_audio_data *audio);
};

WASAPISource::WASAPISource(obs_data_t *settings, obs_source_t *source_,
		bool input)
	: source          (source_),
	  isInputDevice   (input)
{
	UpdateSettings(settings);

	stopSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!stopSignal.Valid())
		throw "Could not create stop signal";

	receiveSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!receiveSignal.Valid())
		throw "Could not create receive signal";

	Start();
}

inline void WASAPISource::Start()
{
	this->InitSpeexAEC();

	if (!TryInitialize()) {
		blog(LOG_INFO, "[WASAPISource::WASAPISource] "
		               "Device '%s' not found.  Waiting for device",
		               device_id.c_str());
		Reconnect();
	}
}

inline void WASAPISource::InitSpeexAEC()
{
	// ����Ĭ�ϵ���������������������...
	m_out_channel_num = 1;
	m_out_sample_rate = 16000;
	// �������趨�Ĳ����ӳ�ʱ��(����)...
	m_horn_delay_ms = 0;
	// ����ÿ�δ����趨������ռ�õ�������(Ĭ��16����)
	m_nSpeexNN = 16 * (m_out_sample_rate / 1000) * m_out_channel_num;
	// ����β��Ϊ�趨����ռ�õ�������(Ĭ��400����) => �����ͬ����Խ�󣬼�����Խ������Ч��Խ��...
	m_nSpeexTAIL = 400 * (m_out_sample_rate / 1000) * m_out_channel_num;
	// ���ݼ������������������Ҫ�õ������ݿռ�...
	m_lpMicBufNN = new short[m_nSpeexNN];
	m_lpHornBufNN = new short[m_nSpeexNN];
	m_lpEchoBufNN = new short[m_nSpeexNN];
	// ��ʼ���������������� => speex
	m_lpSpeexEcho = speex_echo_state_init(m_nSpeexNN, m_nSpeexTAIL);
	m_lpSpeexDen = speex_preprocess_state_init(m_nSpeexNN, m_out_sample_rate);
	speex_echo_ctl(m_lpSpeexEcho, SPEEX_ECHO_SET_SAMPLING_RATE, &m_out_sample_rate);
	speex_preprocess_ctl(m_lpSpeexDen, SPEEX_PREPROCESS_SET_ECHO_STATE, m_lpSpeexEcho);
	// ��ʼ���������ζ��ж���...
	circlebuf_init(&m_circle_mic);
	circlebuf_init(&m_circle_horn);
	// ��ʼ�����������������...
	pthread_mutex_init_value(&m_SpeexMutex);
}

inline void WASAPISource::UnInitSpeexAEC()
{
	// �ͷ���˷����������Ƶת����...
	if (m_mic_resampler != nullptr) {
		audio_resampler_destroy(m_mic_resampler);
		m_mic_resampler = nullptr;
	}
	// �ͷ�����������������Ƶת����...
	if (m_horn_resampler != nullptr) {
		audio_resampler_destroy(m_horn_resampler);
		m_horn_resampler = nullptr;
	}
	// �ͷŻ���������ض���...
	if (m_lpSpeexEcho != NULL) {
		speex_echo_state_destroy(m_lpSpeexEcho);
		m_lpSpeexEcho = NULL;
	}
	if (m_lpSpeexDen != NULL) {
		speex_preprocess_state_destroy(m_lpSpeexDen);
		m_lpSpeexDen = NULL;
	}
	// �ͷ���Ƶ���ζ���...
	circlebuf_free(&m_circle_mic);
	circlebuf_free(&m_circle_horn);
	// �ͷŻ�������ʹ�õ��Ļ���ռ�...
	if (m_lpMicBufNN != NULL) {
		delete[] m_lpMicBufNN;
		m_lpMicBufNN = NULL;
	}
	if (m_lpHornBufNN != NULL) {
		delete[] m_lpHornBufNN;
		m_lpHornBufNN = NULL;
	}
	if (m_lpEchoBufNN != NULL) {
		delete[] m_lpEchoBufNN;
		m_lpEchoBufNN = NULL;
	}
	// �ͷŻ��������������...
	pthread_mutex_destroy(&m_SpeexMutex);
}

inline void WASAPISource::Stop()
{
	SetEvent(stopSignal);
	if (active) {
		blog(LOG_INFO, "WASAPI: Device '%s' Terminated",
				device_name.c_str());
		WaitForSingleObject(captureThread, INFINITE);
	}

	if (reconnecting)
		WaitForSingleObject(reconnectThread, INFINITE);

	ResetEvent(stopSignal);

	UnInitSpeexAEC();
}

inline WASAPISource::~WASAPISource()
{
	Stop();
}

void WASAPISource::UpdateSettings(obs_data_t *settings)
{
	device_id       = obs_data_get_string(settings, OPT_DEVICE_ID);
	useDeviceTiming = obs_data_get_bool(settings, OPT_USE_DEVICE_TIMING);
	isDefaultDevice = _strcmpi(device_id.c_str(), "default") == 0;
}

void WASAPISource::Update(obs_data_t *settings)
{
	string newDevice = obs_data_get_string(settings, OPT_DEVICE_ID);
	bool restart = newDevice.compare(device_id) != 0;

	if (restart)
		Stop();

	UpdateSettings(settings);

	if (restart)
		Start();
}

bool WASAPISource::InitDevice(IMMDeviceEnumerator *enumerator)
{
	HRESULT res;

	if (isDefaultDevice) {
		res = enumerator->GetDefaultAudioEndpoint(
				isInputDevice ? eCapture        : eRender,
				isInputDevice ? eCommunications : eConsole,
				device.Assign());
	} else {
		wchar_t *w_id;
		os_utf8_to_wcs_ptr(device_id.c_str(), device_id.size(), &w_id);

		res = enumerator->GetDevice(w_id, device.Assign());

		bfree(w_id);
	}

	return SUCCEEDED(res);
}

#define BUFFER_TIME_100NS (5*10000000)

void WASAPISource::InitClient()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT                    res;
	DWORD                      flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			nullptr, (void**)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	InitFormat(wfex);

	if (!isInputDevice)
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

	res = client->Initialize(
			AUDCLNT_SHAREMODE_SHARED, flags,
			BUFFER_TIME_100NS, 0, wfex, nullptr);
	if (FAILED(res))
		throw HRError("Failed to get initialize audio client", res);
}

void WASAPISource::InitRender()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT                    res;
	LPBYTE                     buffer;
	UINT32                     frames;
	ComPtr<IAudioClient>       client;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			nullptr, (void**)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	res = client->Initialize(
			AUDCLNT_SHAREMODE_SHARED, 0,
			BUFFER_TIME_100NS, 0, wfex, nullptr);
	if (FAILED(res))
		throw HRError("Failed to get initialize audio client", res);

	/* Silent loopback fix. Prevents audio stream from stopping and */
	/* messing up timestamps and other weird glitches during silence */
	/* by playing a silent sample all over again. */

	res = client->GetBufferSize(&frames);
	if (FAILED(res))
		throw HRError("Failed to get buffer size", res);

	res = client->GetService(__uuidof(IAudioRenderClient),
		(void**)render.Assign());
	if (FAILED(res))
		throw HRError("Failed to get render client", res);

	res = render->GetBuffer(frames, &buffer);
	if (FAILED(res))
		throw HRError("Failed to get buffer", res);

	memset(buffer, 0, frames*wfex->nBlockAlign);

	render->ReleaseBuffer(frames, 0);
}

static speaker_layout ConvertSpeakerLayout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:          return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:         return SPEAKERS_4POINT0;
	case KSAUDIO_SPEAKER_4POINT1:          return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1_SURROUND: return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND: return SPEAKERS_7POINT1;
	}

	return (speaker_layout)channels;
}

void WASAPISource::InitFormat(WAVEFORMATEX *wfex)
{
	DWORD layout = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE*)wfex;
		layout = ext->dwChannelMask;
	}

	/* WASAPI is always float */
	//sampleRate = wfex->nSamplesPerSec;
	//format     = AUDIO_FORMAT_FLOAT;
	//speakers   = ConvertSpeakerLayout(layout, wfex->nChannels);

	// �趨����������Ҫ����Ƶ������ʽ��16λ����������16000������...
	m_echo_sample_info.format = AUDIO_FORMAT_16BIT;
	m_echo_sample_info.speakers = (speaker_layout)m_out_channel_num;
	m_echo_sample_info.samples_per_sec = m_out_sample_rate;

	/* WASAPI is always float for input */
	// ��ϵͳ���õ��л�ȡ��˷�ԭʼ������ʽ...
	m_mic_sample_info.format = AUDIO_FORMAT_FLOAT;
	m_mic_sample_info.speakers = ConvertSpeakerLayout(layout, wfex->nChannels);
	m_mic_sample_info.samples_per_sec = wfex->nSamplesPerSec;
	// ������˷�ԭʼ���ݵ��ز������󣬽���˷�ɼ�������������ת���ɻ���������Ҫ��������ʽ...
	m_mic_resampler = audio_resampler_create(&m_echo_sample_info, &m_mic_sample_info);
}

void WASAPISource::InitCapture()
{
	HRESULT res = client->GetService(__uuidof(IAudioCaptureClient),
			(void**)capture.Assign());
	if (FAILED(res))
		throw HRError("Failed to create capture context", res);

	res = client->SetEventHandle(receiveSignal);
	if (FAILED(res))
		throw HRError("Failed to set event handle", res);

	captureThread = CreateThread(nullptr, 0,
			WASAPISource::CaptureThread, this,
			0, nullptr);
	if (!captureThread.Valid())
		throw "Failed to create capture thread";

	client->Start();
	active = true;

	blog(LOG_INFO, "WASAPI: Device '%s' initialized", device_name.c_str());
}

void WASAPISource::Initialize()
{
	ComPtr<IMMDeviceEnumerator> enumerator;
	HRESULT res;

	res = CoCreateInstance(__uuidof(MMDeviceEnumerator),
			nullptr, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void**)enumerator.Assign());
	if (FAILED(res))
		throw HRError("Failed to create enumerator", res);

	if (!InitDevice(enumerator))
		return;

	device_name = GetDeviceName(device);

	InitClient();
	if (!isInputDevice) InitRender();
	InitCapture();
}

bool WASAPISource::TryInitialize()
{
	try {
		Initialize();

	} catch (HRError error) {
		if (previouslyFailed)
			return active;

		blog(LOG_WARNING, "[WASAPISource::TryInitialize]:[%s] %s: %lX",
				device_name.empty() ?
					device_id.c_str() : device_name.c_str(),
				error.str, error.hr);

	} catch (const char *error) {
		if (previouslyFailed)
			return active;

		blog(LOG_WARNING, "[WASAPISource::TryInitialize]:[%s] %s",
				device_name.empty() ?
					device_id.c_str() : device_name.c_str(),
				error);
	}

	previouslyFailed = !active;
	return active;
}

void WASAPISource::Reconnect()
{
	reconnecting = true;
	reconnectThread = CreateThread(nullptr, 0,
			WASAPISource::ReconnectThread, this,
			0, nullptr);

	if (!reconnectThread.Valid())
		blog(LOG_WARNING, "[WASAPISource::Reconnect] "
		                "Failed to initialize reconnect thread: %lu",
		                 GetLastError());
}

static inline bool WaitForSignal(HANDLE handle, DWORD time)
{
	return WaitForSingleObject(handle, time) != WAIT_TIMEOUT;
}

#define RECONNECT_INTERVAL 3000

DWORD WINAPI WASAPISource::ReconnectThread(LPVOID param)
{
	WASAPISource *source = (WASAPISource*)param;

	os_set_thread_name("win-wasapi: reconnect thread");

	while (!WaitForSignal(source->stopSignal, RECONNECT_INTERVAL)) {
		if (source->TryInitialize())
			break;
	}

	source->reconnectThread = nullptr;
	source->reconnecting = false;
	return 0;
}

static void doSaveAudioPCM(uint8_t * lpBufData, int nBufSize, int nAudioRate, int nAudioChannel)
{
	// ע�⣺PCM���ݱ����ö����Ʒ�ʽ���ļ�...
	char szFullPath[MAX_PATH] = { 0 };
	sprintf(szFullPath, "F:/MP4/PCM/mic_%d_%d_short.pcm", nAudioRate, nAudioChannel);
	FILE * lpFile = fopen(szFullPath, "ab+");
	// ���ļ��ɹ�����ʼд����ƵPCM��������...
	if (lpFile != NULL) {
		fwrite(lpBufData, nBufSize, 1, lpFile);
		fclose(lpFile);
	}
}

bool WASAPISource::ProcessCaptureData()
{
	HRESULT res;
	LPBYTE  buffer;
	UINT32  frames;
	DWORD   flags;
	UINT64  pos, ts;
	UINT    captureSize = 0;

	while (true) {
		res = capture->GetNextPacketSize(&captureSize);

		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
						"[WASAPISource::GetCaptureData]"
						" capture->GetNextPacketSize"
						" failed: %lX", res);
			return false;
		}

		if (!captureSize)
			break;

		res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
						"[WASAPISource::GetCaptureData]"
						" capture->GetBuffer"
						" failed: %lX", res);
			return false;
		}

		uint64_t  ts_offset = 0;
		uint32_t  output_frames = 0;
		uint8_t * output_data[MAX_AV_PLANES] = { 0 };
		// ��ԭʼ��˷���Ƶ������ʽ��ת���ɻ���������Ҫ��������ʽ��ת���ɹ����뻷�ζ���...
		if (audio_resampler_resample(m_mic_resampler, output_data, &output_frames, &ts_offset, (const uint8_t *const *)&buffer, frames)) {
			int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
			circlebuf_push_back(&m_circle_mic, output_data[0], cur_data_size);
		}
		// ���л�����������...
		this->doEchoCancel(ts);
		// �ͷ�Ӳ���Ĳ�׽����...
		capture->ReleaseBuffer(frames);
	}

	return true;
}

void WASAPISource::doEchoCancel(UINT64 ts)
{
	// ע�⣺���ﲢ�����������������Ƿ��㹻���������ܽ�����˷���ʱ...
	// ��˷����ݱ������һ������Ԫ => ������ * ÿ������ռ���ֽ���...
	size_t nNeedBufBytes = m_nSpeexNN * sizeof(short);
	if (m_circle_mic.size < nNeedBufBytes)
		return;
	// ��ȡ��˷���Ҫ�Ĵ����������� => ע���Ƕ�ȡ�ֽ���������������...
	circlebuf_pop_front(&m_circle_mic, m_lpMicBufNN, nNeedBufBytes);
	// ��������������㹻����ȡ���������ݵ��������������浱��..
	if (m_circle_horn.size >= nNeedBufBytes) {
		// �����������ζ��е��ж�ȡ���ݣ�������Ҫ�̻߳���...
		pthread_mutex_lock(&m_SpeexMutex);
		circlebuf_pop_front(&m_circle_horn, m_lpHornBufNN, nNeedBufBytes);
		pthread_mutex_unlock(&m_SpeexMutex);
		// ��˷����������������׼���������Ƚ��л����������ٽ���һ�����⴦��...
		speex_echo_cancellation(m_lpSpeexEcho, m_lpMicBufNN, m_lpHornBufNN, m_lpEchoBufNN);
		speex_preprocess_run(m_lpSpeexDen, m_lpEchoBufNN);
		// ����������PCM���ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
		doSaveAudioPCM((uint8_t*)m_lpHornBufNN, nNeedBufBytes, m_out_sample_rate, 1);
	} else {
		// ���������ݲ���������˷���������ֱ�ӵ��ɻ������������������...
		memcpy(m_lpEchoBufNN, m_lpMicBufNN, nNeedBufBytes);
	}
	// �Ի�������������ݽ��д��̴��� => �����ö����Ʒ�ʽ���ļ�...
	doSaveAudioPCM((uint8_t*)m_lpMicBufNN, nNeedBufBytes, m_out_sample_rate, 0);
	doSaveAudioPCM((uint8_t*)m_lpEchoBufNN, nNeedBufBytes, m_out_sample_rate, 2);

	// �����������֮�����Ƶ����...
	obs_source_audio data = {};
	data.data[0] = (uint8_t*)m_lpEchoBufNN;
	data.frames = m_nSpeexNN;
	data.speakers = m_echo_sample_info.speakers;
	data.samples_per_sec = m_echo_sample_info.samples_per_sec;
	data.format = m_echo_sample_info.format;
	data.timestamp = useDeviceTiming ? ts * 100 : os_gettime_ns();
	// ϵͳ��ʱ��Ҫһ������...?
	if (!useDeviceTiming) {
		data.timestamp -= (uint64_t)m_nSpeexNN * 1000000000ULL / (uint64_t)m_mic_sample_info.samples_per_sec;
	}
	// ��obs�ϲ�������ݵĴ��ݹ���...
	obs_source_output_audio(source, &data);
}

obs_audio_data * WASAPISource::AudioEchoCancelFilter(obs_audio_data *audio)
{
	// �����׽�豸��û�г�ʼ����ϣ����ܽ��л��������������...
	if (m_mic_resampler == nullptr)
		return NULL;
	// ע�⣺ʵ�ʴ��ݵĽṹ�� obs_source_audio����� wasapi-output.c::doEnumSources()
	obs_source_audio * lpSourceAudio = (obs_source_audio*)audio;
	// �����������ת������Ϊ�գ���Ҫ����ת����������...
	if (m_horn_resampler == nullptr) {
		m_horn_sample_info.samples_per_sec = lpSourceAudio->samples_per_sec;
		m_horn_sample_info.speakers = lpSourceAudio->speakers;
		m_horn_sample_info.format = lpSourceAudio->format;
		// ����������ԭʼ���ݵ��ز������󣬽��������ɼ�������������ת���ɻ���������Ҫ��������ʽ...
		m_horn_resampler = audio_resampler_create(&m_echo_sample_info, &m_horn_sample_info);
	}
	uint64_t  ts_offset = 0;
	uint32_t  output_frames = 0;
	uint8_t * output_data[MAX_AV_PLANES] = { 0 };
	// �������ԭʼ��Ƶ������ʽ����ͳһ�ĸ�ʽת����ת���ɹ������뻷�ζ���...
	if (audio_resampler_resample(m_horn_resampler, output_data, &output_frames, &ts_offset, lpSourceAudio->data, lpSourceAudio->frames)) {
		// ע�⣺������Ҫ���л��Ᵽ�����Ƕ��߳����ݷ���...
		pthread_mutex_lock(&m_SpeexMutex);
		int cur_data_size = get_audio_size(m_echo_sample_info.format, m_echo_sample_info.speakers, output_frames);
		circlebuf_push_back(&m_circle_horn, output_data[0], cur_data_size);
		pthread_mutex_unlock(&m_SpeexMutex);
	}
	// ���Է��ؿ�ָ��...
	return audio;
}

static inline bool WaitForCaptureSignal(DWORD numSignals, const HANDLE *signals,
		DWORD duration)
{
	DWORD ret;
	ret = WaitForMultipleObjects(numSignals, signals, false, duration);

	return ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT;
}

DWORD WINAPI WASAPISource::CaptureThread(LPVOID param)
{
	WASAPISource *source   = (WASAPISource*)param;
	bool         reconnect = false;

	/* Output devices don't signal, so just make it check every 10 ms */
	DWORD        dur       = source->isInputDevice ? INFINITE : 10;

	HANDLE sigs[2] = {
		source->receiveSignal,
		source->stopSignal
	};

	os_set_thread_name("win-wasapi: capture thread");

	while (WaitForCaptureSignal(2, sigs, dur)) {
		if (!source->ProcessCaptureData()) {
			reconnect = true;
			break;
		}
	}

	source->client->Stop();

	source->captureThread = nullptr;
	source->active        = false;

	if (reconnect) {
		blog(LOG_INFO, "Device '%s' invalidated.  Retrying",
				source->device_name.c_str());
		source->Reconnect();
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static const char *GetWASAPIInputName(void*)
{
	return obs_module_text("AudioInput");
}

static const char *GetWASAPIOutputName(void*)
{
	return obs_module_text("AudioOutput");
}

static void GetWASAPIDefaultsInput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, false);
}

static void GetWASAPIDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, true);
}

static void *CreateWASAPISource(obs_data_t *settings, obs_source_t *source,
		bool input)
{
	try {
		return new WASAPISource(settings, source, input);
	} catch (const char *error) {
		blog(LOG_ERROR, "[CreateWASAPISource] %s", error);
	}

	return nullptr;
}

static void *CreateWASAPIInput(obs_data_t *settings, obs_source_t *source)
{
	return CreateWASAPISource(settings, source, true);
}

static void *CreateWASAPIOutput(obs_data_t *settings, obs_source_t *source)
{
	return CreateWASAPISource(settings, source, false);
}

static void DestroyWASAPISource(void *obj)
{
	delete static_cast<WASAPISource*>(obj);
}

static void UpdateWASAPISource(void *obj, obs_data_t *settings)
{
	static_cast<WASAPISource*>(obj)->Update(settings);
}

// ����α�������ӿڣ�ר�Ź��� wasapi-output.c ʹ�õģ����Ͷ�ݵ�����������Ƶ����...
static obs_audio_data * AudioEchoCancelFilter(void *obj, obs_audio_data *audio)
{
	return static_cast<WASAPISource*>(obj)->AudioEchoCancelFilter(audio);
}

static obs_properties_t *GetWASAPIProperties(bool input)
{
	obs_properties_t *props = obs_properties_create();
	vector<AudioDeviceInfo> devices;

	obs_property_t *device_prop = obs_properties_add_list(props,
			OPT_DEVICE_ID, obs_module_text("Device"),
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	GetWASAPIAudioDevices(devices, input);

	if (devices.size())
		obs_property_list_add_string(device_prop,
				obs_module_text("Default"), "default");

	for (size_t i = 0; i < devices.size(); i++) {
		AudioDeviceInfo &device = devices[i];
		obs_property_list_add_string(device_prop,
				device.name.c_str(), device.id.c_str());
	}

	obs_properties_add_bool(props, OPT_USE_DEVICE_TIMING,
			obs_module_text("UseDeviceTiming"));

	return props;
}

static obs_properties_t *GetWASAPIPropertiesInput(void *)
{
	return GetWASAPIProperties(true);
}

static obs_properties_t *GetWASAPIPropertiesOutput(void *)
{
	return GetWASAPIProperties(false);
}

void RegisterWASAPIInput()
{
	obs_source_info info = {};
	info.id              = "wasapi_input_capture";
	info.type            = OBS_SOURCE_TYPE_INPUT;
	info.output_flags    = OBS_SOURCE_AUDIO |
	                       OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name        = GetWASAPIInputName;
	info.create          = CreateWASAPIInput;
	info.destroy         = DestroyWASAPISource;
	info.update          = UpdateWASAPISource;
	info.get_defaults    = GetWASAPIDefaultsInput;
	info.get_properties  = GetWASAPIPropertiesInput;
	info.filter_audio    = AudioEchoCancelFilter;
	obs_register_source(&info);
}

void RegisterWASAPIOutput()
{
	obs_source_info info = {};
	info.id              = "wasapi_output_capture";
	info.type            = OBS_SOURCE_TYPE_INPUT;
	info.output_flags    = OBS_SOURCE_AUDIO |
	                       OBS_SOURCE_DO_NOT_DUPLICATE |
	                       OBS_SOURCE_DO_NOT_SELF_MONITOR;
	info.get_name        = GetWASAPIOutputName;
	info.create          = CreateWASAPIOutput;
	info.destroy         = DestroyWASAPISource;
	info.update          = UpdateWASAPISource;
	info.get_defaults    = GetWASAPIDefaultsOutput;
	info.get_properties  = GetWASAPIPropertiesOutput;
	obs_register_source(&info);
}
