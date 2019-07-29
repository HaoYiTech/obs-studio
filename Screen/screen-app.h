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

#include "json.h"
#include "HYCommon.h"

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

class CLoginMini;
class OBSBasicPreview;
class CScreenApp : public QApplication {
	Q_OBJECT
public:
	CScreenApp(int &argc, char **argv, profiler_name_store_t *store);
	~CScreenApp();
public:
	void AppInit();
	bool OBSInit();
	void doLoginInit();
	void doProcessCmdLine(int argc, char * argv[]);
public:
	static string getJsonString(Json::Value & inValue);
	static void RenderMain(void *data, uint32_t cx, uint32_t cy);
	static void ReceiveRawVideo(void *param, struct video_data *frame);
public slots:
	void onTriggerMiniSuccess();
public:
	void	 SetUserNameStr(const string & strUser) { m_strUserName = strUser; }
	void	 SetRoomIDStr(const string & strRoom) { m_strRoomID = strRoom; }
	void	 SetUdpAddr(const string & strAddr) { m_strUdpAddr = strAddr; }
	void     SetUdpPort(int nPort) { m_nUdpPort = nPort; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
	string & GetUdpAddr() { return m_strUdpAddr; }
	int		 GetUdpPort() { return m_nUdpPort; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	string & GetWebClass() { return m_strWebClass; }
	string & GetWebCenter() { return m_strWebCenter; }
	int      GetClientType() { return kClientScreen; }
	string & GetRoomIDStr() { return m_strRoomID; }
	string & GetUserNameStr() { return m_strUserName; }
	bool     IsDebugMode() { return m_bIsDebugMode; }
public:
	const char *GetRenderModule() const;
	std::string GetVersionString() const;
	bool TranslateString(const char *lookupVal, const char **out) const;
	inline config_t *GlobalConfig() const { return m_globalConfig; }
	inline lookup_t *GetTextLookup() const { return m_textLookup; }
	inline const char *GetString(const char *lookupVal) const {
		return m_textLookup.GetString(lookupVal);
	}
	profiler_name_store_t *GetProfilerNameStore() const {
		return profilerNameStore;
	}
private:
	void    LogScenes();
	int     ResetVideo();
	bool	InitLocale();
	void    ClearSceneData();
	void    doCreateDisplay();
	bool	InitGlobalConfig();
	bool	InitGlobalConfigDefaults();
	bool	doSaveMemJpg(video_data * frame);
	void    ResizePreview(uint32_t cx, uint32_t cy);
	bool    doSaveFileJpg(uint8_t * inData, int nSize);
	bool    doSendScreenSnap(uint8_t * inData, int nSize);
	void    AddSceneItem(obs_scene_t *scene, obs_source_t *source);
private:
	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	profiler_name_store_t  *   profilerNameStore = nullptr;
	QPointer<CLoginMini>       m_LoginMini = nullptr;       // С�����¼����
	QPointer<OBSBasicPreview>  m_preview = nullptr;         // Ԥ�����ڶ���...
	ConfigFile      m_globalConfig;               // ȫ�����ö���...
	TextLookup      m_textLookup;                 // ���ַ������...
	OBSContext      m_obsContext;                 // �ͷŽṹ����...
	obs_source_t *  m_obsSource = nullptr;        // Ψһ������Դ...
	obs_scene_t  *  m_obsScene = nullptr;         // Ψһ������...
	string          m_strWebCenter;               // ����������վ��ַ...
	string          m_strWebClass;				  // �����ƽ�����վ��ַ...
	string          m_strLocale;                  // �������� => zh-CN
	bool            m_bIsDebugMode = false;       // �Ƿ��ǵ���ģʽ => ���ص����Է�����...
	bool            m_bIsSaveFile = false;        // �Ƿ���JPG�ļ���־ => Ĭ��False...
	bool            m_bIsObsInit = false;         // �Ƿ��Ѿ���ʼ�����OBS��־...
	int             m_nFPS = 5;                   // ÿ��֡�ʣ�����CPUʹ����...
	int             m_snap_second = 5;            // ��ͼ���ʱ��(��)...
	float           m_qCompress = 0.5f;           // JPGѹ������...
	uint64_t        m_start_time_ns = 0;          // Ĭ�ϵ�����ʱ��...
	float           previewScale = 0.0f;          // Ԥ��������...
	int             previewX = 0, previewY = 0;   // Ԥ��λ��...
	int             previewCX = 0, previewCY = 0; // Ԥ����С...

	string			m_strTrackerAddr;				// FDFS-Tracker��IP��ַ...
	int				m_nTrackerPort = 0;				// FDFS-Tracker�Ķ˿ڵ�ַ...
	string			m_strRemoteAddr;				// Զ����ת��������IP��ַ...
	int				m_nRemotePort = 0;				// Զ����ת�������Ķ˿ڵ�ַ...
	string			m_strUdpAddr;					// Զ��UDP��������IP��ַ...
	int				m_nUdpPort = 0;					// Զ��UDP�������Ķ˿ڵ�ַ...
	string          m_strUserName;
	string          m_strRoomID;

	friend class CLoginMini;
};

inline CScreenApp *App() { return static_cast<CScreenApp*>(qApp); }
inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);
bool WindowPositionValid(QRect rect);
