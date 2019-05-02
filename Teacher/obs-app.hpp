
#pragma once

#include <QNetworkAccessManager>
#include <QApplication>
#include <QTranslator>
#include <QPointer>
#include <obs.hpp>
#include <util/lexer.h>
#include <util/profiler.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include "json.h"

#include "HYDefine.h"
#include "window-main.hpp"

using namespace std;

std::string CurrentTimeString();
std::string CurrentDateTimeString();
std::string GenerateTimeDateFilename(const char *extension, bool noSpace = false);
std::string GenerateSpecifiedFilename(const char *extension, bool noSpace, const char *format);
QObject *CreateShortcutFilter();

struct BaseLexer {
	lexer lex;
public:
	inline BaseLexer() { lexer_init(&lex); }
	inline ~BaseLexer() { lexer_free(&lex); }
	operator lexer*() { return &lex; }
};

class OBSTranslator : public QTranslator {
	Q_OBJECT

public:
	virtual bool isEmpty() const override { return false; }

	virtual QString translate(const char *context, const char *sourceText,
		const char *disambiguation, int n) const override;
};

class CLoginMini;
class QListWidget;
class CRemoteSession;
class CTrackerSession;
class CStorageSession;
class OBSApp : public QApplication {
	Q_OBJECT
private:
	std::string                    m_strTrackerAddr;           // FDFS-Tracker��IP��ַ...
	int                            m_nTrackerPort;             // FDFS-Tracker�Ķ˿ڵ�ַ...
	std::string                    m_strRemoteAddr;            // Զ��UDPServer��TCP��ַ...
	int                            m_nRemotePort;              // Զ��UDPServer��TCP�˿�...
	std::string                    m_strUdpAddr;               // Զ��UDPServer��UDP��ַ...
	int                            m_nUdpPort;                 // Զ��UDPServer��UDP�˿�...
	std::string                    m_strRoomID;                // ��¼�ķ����...
	std::string                    m_strMacAddr;               // ����MAC��ַ...
	std::string                    m_strIPAddr;                // ����IP��ַ...
	std::string                    m_strWebCenter;             // ����������վ��ַ...
	std::string                    m_strWebClass;              // �����ƽ�����վ��ַ...
	int                            m_nFastTimer;               // �ֲ�ʽ�洢����ת���Ӽ��ʱ��...
	int                            m_nFlowTimer;               // ����ͳ�Ƽ��ʱ��...
	int                            m_nOnLineTimer;             // ��ת���������߼��ʱ��...
	int                            m_nRtpTCPSockFD;            // CRemoteSession�ڷ������˵��׽��ֺ���...
	int                            m_nRtpDBCameraID;           // ��ǰ���ڹۿ��Ļ������ҵ�����ͷ���...
	int                            m_nDBFlowID;                // �ӷ�������ȡ��������ͳ�����ݿ���...
	int                            m_nDBUserID;                // �ѵ�¼�û������ݿ���...
	bool                           m_bIsDebugMode;             // �Ƿ��ǵ���ģʽ => ���ص����Է�����...
	std::string                    locale;
	std::string	                   theme;
	ConfigFile                     globalConfig;
	TextLookup                     textLookup;
	OBSContext                     obsContext;
	QPointer<OBSMainWindow>        mainWindow;                 // �����ڶ���
	QPointer<CLoginMini>           m_LoginMini;                // С�����¼����
	QPointer<CTrackerSession>      m_TrackerSession;           // For Tracker-Server
	QPointer<CStorageSession>      m_StorageSession;           // For Storage-Server
	QPointer<CRemoteSession>       m_RemoteSession;            // For UDP-Server
	QNetworkAccessManager          m_objNetManager;            // QT ����������...

	profiler_name_store_t          *profilerNameStore = nullptr;

	os_inhibit_t                   *sleepInhibitor = nullptr;
	int                            sleepInhibitRefs = 0;

	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
private:
	bool InitGlobalConfig();
	bool InitGlobalConfigDefaults();
	bool InitLocale();
	bool InitTheme();
	bool InitMacIPAddr();
	void FindRtpSource(int & outDBCameraID, int & outSceneItemID, bool & outHasRecvThread);
public:
	static char * GetServerDNSName();
public:
	bool     IsClassHttps() { return ((strnicmp(m_strWebClass.c_str(), "https://", strlen("https://")) == 0) ? true : false); }
	bool     IsDebugMode() { return m_bIsDebugMode; }
	int      GetClientType() { return kClientTeacher; }
	string & GetWebClass() { return m_strWebClass; }
	string & GetWebCenter() { return m_strWebCenter; }
	string & GetLocalIPAddr() { return m_strIPAddr; }
	string & GetLocalMacAddr() { return m_strMacAddr; }
	string & GetRoomIDStr() { return m_strRoomID; }
public:
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetUdpAddr() { return m_strUdpAddr; }
	int		 GetUdpPort() { return m_nUdpPort; }
	int      GetRtpTCPSockFD() { return m_nRtpTCPSockFD; }
	int      GetRtpDBCameraID() { return m_nRtpDBCameraID; }
	int      GetDBFlowID() { return m_nDBFlowID; }

	void     SetDBFlowID(int nDBFlowID) { m_nDBFlowID = nDBFlowID; }
	void     SetRtpDBCameraID(int nDBCameraID) { m_nRtpDBCameraID = nDBCameraID; }
	void     SetRtpTCPSockFD(int nTCPSockFD) { m_nRtpTCPSockFD = nTCPSockFD; }
	void	 SetUdpAddr(const string & strAddr) { m_strUdpAddr = strAddr; }
	void     SetUdpPort(int nPort) { m_nUdpPort = nPort; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
public:
	static string getJsonString(Json::Value & inValue);
public slots:
	void onTriggerMiniSuccess();
	void onReplyFinished(QNetworkReply *reply);
public:
	OBSApp(int &argc, char **argv, profiler_name_store_t *store);
	~OBSApp();

	void AppInit();
	bool OBSInit();
	
	void doLoginInit();
	void doLogoutEvent();
	void doProcessCmdLine(int argc, char * argv[]);

	bool doSendCameraOnLineListCmd();
	bool doSendCameraLiveStopCmd(int nDBCameraID, int nSceneItemID);
	bool doSendCameraLiveStartCmd(int nDBCameraID, int nSceneItemID);
	bool doSendCameraPTZCmd(int nDBCameraID, int nCmdID, int nSpeedVal);

	void doCheckTracker();
	void doCheckStorage();
	void doCheckRemote();
	void doCheckFDFS();
	void doCheckOnLine();
	void doCheckRoomFlow();
	void doCheckRtpSource();
	bool doFindOneFile(char * inPath, int inSize, const char * inExtend);
	void doWebSaveFDFS(char * lpFileName, char * lpPathFDFS, int64_t llFileSize);

	void timerEvent(QTimerEvent * inEvent);

	inline QMainWindow *GetMainWindow() const { return mainWindow.data(); }
	inline config_t *GlobalConfig() const { return globalConfig; }

	inline const char *GetLocale() const
	{
		return locale.c_str();
	}

	inline const char *GetTheme() const { return theme.c_str(); }
	bool SetTheme(std::string name, std::string path = "");

	inline lookup_t *GetTextLookup() const { return textLookup; }

	inline const char *GetString(const char *lookupVal) const
	{
		return textLookup.GetString(lookupVal);
	}

	bool TranslateString(const char *lookupVal, const char **out) const;

	profiler_name_store_t *GetProfilerNameStore() const
	{
		return profilerNameStore;
	}

	const char *GetLastLog() const;
	const char *GetCurrentLog() const;

	const char *GetLastCrashLog() const;

	std::string GetVersionString() const;
	bool IsPortableMode();

	const char *GetNSFilter() const;

	const char *DShowInputSource() const;
	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;
	const char *InteractRtpSource() const;

	const char *GetRenderModule() const;

	inline void IncrementSleepInhibition()
	{
		if (!sleepInhibitor) return;
		if (sleepInhibitRefs++ == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, true);
	}

	inline void DecrementSleepInhibition()
	{
		if (!sleepInhibitor) return;
		if (sleepInhibitRefs == 0) return;
		if (--sleepInhibitRefs == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, false);
	}

	inline void PushUITranslation(obs_frontend_translate_ui_cb cb)
	{
		translatorHooks.emplace_front(cb);
	}

	inline void PopUITranslation()
	{
		translatorHooks.pop_front();
	}
};

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);

int GetProgramDataPath(char *path, size_t size, const char *name);
char *GetProgramDataPathPtr(const char *name);

inline OBSApp *App() { return static_cast<OBSApp*>(qApp); }

inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }

std::vector<std::pair<std::string, std::string>> GetLocaleNames();
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

bool GetFileSafeName(const char *name, std::string &file);
bool GetClosestUnusedFileName(std::string &path, const char *extension);

bool WindowPositionValid(QRect rect);

static inline int GetProfilePath(char *path, size_t size, const char *file)
{
	OBSMainWindow *window = reinterpret_cast<OBSMainWindow*>(App()->GetMainWindow());
	return window->GetProfilePath(path, size, file);
}

extern bool portable_mode;
extern bool opt_start_streaming;
extern bool opt_start_recording;
extern bool opt_start_replaybuffer;
extern bool opt_minimize_tray;
extern bool opt_studio_mode;
extern bool opt_allow_opengl;
extern bool opt_always_on_top;
extern std::string opt_starting_scene;
