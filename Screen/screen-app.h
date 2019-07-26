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
public slots:
	void onTriggerMiniSuccess();
public:
	string & GetWebClass() { return m_strWebClass; }
	string & GetWebCenter() { return m_strWebCenter; }
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
	void    ResizePreview(uint32_t cx, uint32_t cy);
	void    AddSceneItem(obs_scene_t *scene, obs_source_t *source);
private:
	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	profiler_name_store_t  *   profilerNameStore = nullptr;
	QPointer<CLoginMini>       m_LoginMini = nullptr;       // С�����¼����
	QPointer<OBSBasicPreview>  m_preview = nullptr;         // Ԥ�����ڶ���...
	ConfigFile                 m_globalConfig;              // ȫ�����ö���...
	TextLookup                 m_textLookup;                // ���ַ������...
	OBSContext                 m_obsContext;                // �ͷŽṹ����...
	obs_source_t        *      m_obsSource = nullptr;       // Ψһ������Դ...
	obs_scene_t         *      m_obsScene = nullptr;        // Ψһ������...
	string                     m_strWebCenter;              // ����������վ��ַ...
	string                     m_strWebClass;				// �����ƽ�����վ��ַ...
	string                     m_strLocale;                 // �������� => zh-CN
	bool                       m_bIsDebugMode;              // �Ƿ��ǵ���ģʽ => ���ص����Է�����...
	int                        m_nFPS = 5;                  // ÿ��֡�ʣ�����CPUʹ����...
	float         previewScale = 0.0f;
	int           previewX = 0, previewY = 0;
	int           previewCX = 0, previewCY = 0;

	friend class CLoginMini;
};

inline CScreenApp *App() { return static_cast<CScreenApp*>(qApp); }
inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);
bool WindowPositionValid(QRect rect);
