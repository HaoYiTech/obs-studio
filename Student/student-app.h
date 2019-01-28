#pragma once

#include <QApplication>
#include <QTranslator>
#include <QPointer>

#include <obs.hpp>
#include <util/lexer.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <deque>

#include "FastSession.h"
#include "window-student-main.h"

using namespace std;

std::string CurrentTimeString();
std::string CurrentDateTimeString();
std::string GenerateTimeDateFilename(const char *extension, bool noSpace = false);
std::string GenerateSpecifiedFilename(const char *extension, bool noSpace, const char *format);

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
	virtual QString translate(const char *context, const char *sourceText, const char *disambiguation, int n) const override;
};

class CRemoteSession;
class LoginWindow;
class CWebThread;
class CStudentApp : public QApplication {
	Q_OBJECT
public:
	CStudentApp(int &argc, char **argv);
	~CStudentApp();
signals:
	void msgFromWebThread(int nMessageID, int nWParam, int nLParam);
	void doEnableCamera(OBSQTDisplay * lpNewDisplay);
public:
	void AppInit();
public:
	void doLoginInit();
	void doLogoutEvent();
	void doReBuildResource();
	void onWebLoadResource();
	void onWebAuthExpired();
	void onUdpRecvThreadStop();
	void doUpdatePTZ(int nDBCameraID);
	void doSaveFocus(OBSQTDisplay * lpNewDisplay);
	void doResetFocus(OBSQTDisplay * lpCurDisplay);
	void doProcessCmdLine(int argc, char * argv[]);
	void doEchoCancel(void * lpBufData, int nBufSize, int nSampleRate, int nChannelNum, int msInSndCardBuf);
public:
	static char * GetServerOS();
	static char * GetServerDNSName();
	static string GetSystemVer();
	static string getJsonString(Json::Value & inValue);
	static string UTF8_ANSI(const char * lpUValue);
	static string ANSI_UTF8(const char * lpSValue);
public slots:
	void doLoginSuccess(string & strRoomID);
public:
	bool     GetAudioHorn() { return m_bHasAudioHorn; }
	void     SetAudioHorn(bool bHasAudioHorn) { m_bHasAudioHorn = bHasAudioHorn; }

	bool     GetAuthLicense() { return m_bAuthLicense; }
	int      GetAuthDays() { return m_nAuthDays; }
	string & GetMultiIPSendAddr() { return m_strMultiIPSendAddr; }
	string & GetAuthExpired() { return m_strAuthExpired; }
	string & GetAuthMacMD5() { return m_strAuthMacMD5; }
	string & GetMainName() { return m_strMainName; }
	int		 GetSnapVal() { return m_nSnapVal; }
	int		 GetMaxCamera() { return m_nMaxCamera; }
	string & GetWebVer() { return m_strWebVer; }
	string & GetWebTag() { return m_strWebTag; }
	string & GetWebName() { return m_strWebName; }
	int		 GetWebType() { return m_nWebType; }
	string & GetUdpAddr() { return m_strUdpAddr; }
	int		 GetUdpPort() { return m_nUdpPort; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	int      GetDBGatherID() { return m_nDBGatherID; }
	int      GetDBHaoYiNodeID() { return m_nDBHaoYiNodeID; }
	int      GetDBHaoYiGatherID() { return m_nDBHaoYiGatherID; }
	int      GetRtpTCPSockFD() { return m_nRtpTCPSockFD; }
	ROLE_TYPE GetRoleType() { return m_nRoleType; }
	QString  GetRoleString();

	int      GetAudioRateIndex();
	int      GetAudioChannelNum() { return m_nAudioOutChannelNum; }
	int      GetAudioSampleRate() { return m_nAudioOutSampleRate; }
	int      GetAudioBitrateAAC() { return m_nAudioOutBitrateAAC; }

	void     SetMultiIPSendAddr(const string & strIPSend) { m_strMultiIPSendAddr = strIPSend; }
	void     SetRoleType(ROLE_TYPE inType) { m_nRoleType = inType; }
	void     SetRtpTCPSockFD(int nTCPSockFD) { m_nRtpTCPSockFD = nTCPSockFD; }
	void	 SetUdpAddr(const string & strAddr) { m_strUdpAddr = strAddr; }
	void     SetUdpPort(int nPort) { m_nUdpPort = nPort; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
	void	 SetWebType(int nWebType) { m_nWebType = nWebType; }
	void	 SetWebName(const string & strWebName) { m_strWebName = strWebName; }
	void	 SetWebTag(const string & strWebTag) { m_strWebTag = strWebTag; }
	void	 SetWebVer(const string & strWebVer) { m_strWebVer = strWebVer; }
	void	 SetDBGatherID(int nDBGatherID) { m_nDBGatherID = nDBGatherID; }
	void	 SetDBHaoYiNodeID(int nDBNodeID) { m_nDBHaoYiNodeID = nDBNodeID; }
	void     SetDBHaoYiGatherID(int nDBGatherID) { m_nDBHaoYiGatherID = nDBGatherID; }

	void	 SetAuthDays(const int nAuthDays) { m_nAuthDays = nAuthDays; }
	void     SetAuthMacMD5(const string & strMacMD5) { m_strAuthMacMD5 = strMacMD5; }
	void	 SetAuthExpired(const string & strExpired) { m_strAuthExpired = strExpired; }
	void	 SetAuthLicense(bool bLicense) { m_bAuthLicense = bLicense; }
	void	 SetMainName(const string & strName) { m_strMainName = strName; }
	void	 SetSnapVal(int nSnapVal) { m_nSnapVal = nSnapVal; }
	void	 SetMaxCamera(int nMaxCamera) { m_nMaxCamera = nMaxCamera; }

	string   GetCameraSName(int nDBCameraID);
	QString  GetCameraQName(int nDBCameraID);
	QString  GetCameraPullUrl(int nDBCameraID);
	string   GetCameraDeviceSN(int nDBCameraID);
	void     doDelCamera(int nDBCameraID);
	
	void	 SetCamera(int nDBCameraID, GM_MapData & inMapData) { m_MapNodeCamera[nDBCameraID] = inMapData; }
	void	 GetCamera(int nDBCameraID, GM_MapData & outMapData) { outMapData = m_MapNodeCamera[nDBCameraID]; }
	GM_MapNodeCamera & GetNodeCamera() { return m_MapNodeCamera; }
	CWebThread * GetWebThread() const { return m_lpWebThread; }
	CRemoteSession * GetRemoteSession() const { return m_RemoteSession.data(); }

	bool     IsClassHttps() { return ((strnicmp(m_strWebClass.c_str(), "https://", strlen("https://")) == 0) ? true : false); }
	bool     IsDebugMode() { return m_bIsDebugMode; }
	int      GetClientType() { return kClientStudent; }
	string & GetWebClass() { return m_strWebClass; }
	string & GetWebCenter() { return m_strWebCenter; }
	string & GetLocalIPAddr() { return m_strIPAddr; }
	string & GetLocalMacAddr() { return m_strMacAddr; }
	string & GetRoomIDStr() { return m_strRoomID; }

	std::string GetVersionString() const;
	bool TranslateString(const char *lookupVal, const char **out) const;
	inline StudentWindow *GetMainWindow() const { return m_studentWindow.data(); }
	inline config_t *GlobalConfig() const { return m_globalConfig; }
	inline lookup_t *GetTextLookup() const { return m_textLookup; }
	inline const char *GetString(const char *lookupVal) const
	{
		return m_textLookup.GetString(lookupVal);
	}

	inline void PushUITranslation(obs_frontend_translate_ui_cb cb)
	{
		translatorHooks.emplace_front(cb);
	}

	inline void PopUITranslation()
	{
		translatorHooks.pop_front();
	}
protected:
	void	doCheckFDFS();
	void	doCheckRemote();
	void	doCheckOnLine();
	void	timerEvent(QTimerEvent * inEvent) override;
	bool    notify(QObject * inObject, QEvent * inEvent) override;
private:
	bool	InitLocale();
	bool	InitGlobalConfig();
	bool	InitGlobalConfigDefaults();
	bool	InitMacIPAddr();
private:
	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	QPointer<CRemoteSession>   m_RemoteSession;
	QPointer<StudentWindow>    m_studentWindow;
	QPointer<LoginWindow>      m_loginWindow;
	ConfigFile                 m_globalConfig;
	TextLookup                 m_textLookup;
	OBSQTDisplay      *        m_lpFocusDisplay;
	CWebThread        *        m_lpWebThread;				// ��վ����߳�...
	ROLE_TYPE                  m_nRoleType;                 // �ն˽�ɫ����...
	string                     m_strRoomID;					// ��¼�ķ����...
	string                     m_strMacAddr;				// ����MAC��ַ...
	string                     m_strIPAddr;					// ����IP��ַ...
	string                     m_strWebCenter;              // ����������վ��ַ...
	string                     m_strWebClass;				// �����ƽ�����վ��ַ...
	int                        m_nFastTimer;				// �ֲ�ʽ�洢����ת���Ӽ��ʱ��...
	int                        m_nOnLineTimer;				// �����������ͷͨ���б�...
	GM_MapNodeCamera           m_MapNodeCamera;				// ���ͨ��������Ϣ(���ݿ�CameraID��
	int					m_nMaxCamera;					// �ܹ�֧�ֵ��������ͷ����Ĭ��Ϊ8����
	string				m_strWebVer;					// ע���ǻ�ȡ����վ�汾
	string				m_strWebTag;					// ע��ʱ��ȡ����վ��־
	string				m_strWebName;					// ע��ʱ��ȡ����վ����
	int					m_nWebType;						// ע��ʱ��ȡ����վ����
	string				m_strTrackerAddr;				// FDFS-Tracker��IP��ַ...
	int					m_nTrackerPort;					// FDFS-Tracker�Ķ˿ڵ�ַ...
	string				m_strRemoteAddr;				// Զ����ת��������IP��ַ...
	int					m_nRemotePort;					// Զ����ת�������Ķ˿ڵ�ַ...
	string				m_strUdpAddr;					// Զ��UDP��������IP��ַ...
	int					m_nUdpPort;						// Զ��UDP�������Ķ˿ڵ�ַ...
	int                 m_nDBGatherID;					// ���ݿ��вɼ��˱��...
	int					m_nDBHaoYiNodeID;				// �����ķ������ϵĽڵ���...
	int                 m_nDBHaoYiGatherID;				// �����ķ������ϵĲɼ��˱��...
	string              m_strMultiIPSendAddr;           // ���ķ������������鲥���ͽӿ�...
	string				m_strAuthExpired;				// ���ķ�������������Ȩ����ʱ��...
	string              m_strAuthMacMD5;                // ���ķ���������������MD5У��...
	bool				m_bAuthLicense;					// ���ķ�����������������Ȩ��־...
	int					m_nAuthDays;					// ���ķ�����������ʣ����Ȩ����...
	string				m_strMainName;					// �����ڱ�������...
	int				    m_nSnapVal;						// ͨ����ͼ���(1~10����)
	int                 m_nRtpTCPSockFD;                // CRemoteSession�ڷ������˵��׽��ֺ���...
	int                 m_nAudioOutSampleRate;          // ��Ƶ���š�ѹ����������� 
	int                 m_nAudioOutChannelNum;          // ��Ƶ���š�ѹ�����������
	int                 m_nAudioOutBitrateAAC;          // ����������AACѹ���������
	bool                m_bHasAudioHorn;                // �������Ƿ��Ѿ�������־
	bool                m_bIsDebugMode;                 // �Ƿ��ǵ���ģʽ => ���ص����Է�����...
};

inline CStudentApp *App() { return static_cast<CStudentApp*>(qApp); }
inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);
bool WindowPositionValid(QRect rect);
