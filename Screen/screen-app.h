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
class CScreenApp : public QApplication {
	Q_OBJECT
public:
	CScreenApp(int &argc, char **argv);
	~CScreenApp();
public:
	void AppInit();
	void doLoginInit();
	void doProcessCmdLine(int argc, char * argv[]);
public:
	static string getJsonString(Json::Value & inValue);
public slots:
	void onTriggerMiniSuccess();
public:
	string & GetWebClass() { return m_strWebClass; }
	string & GetWebCenter() { return m_strWebCenter; }
public:
	std::string GetVersionString() const;
	bool TranslateString(const char *lookupVal, const char **out) const;
	inline config_t *GlobalConfig() const { return m_globalConfig; }
	inline lookup_t *GetTextLookup() const { return m_textLookup; }
	inline const char *GetString(const char *lookupVal) const {
		return m_textLookup.GetString(lookupVal);
	}
private:
	bool	InitLocale();
	bool	InitGlobalConfig();
	bool	InitGlobalConfigDefaults();
private:
	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	QPointer<CLoginMini>       m_LoginMini;                 // С�����¼����
	ConfigFile                 m_globalConfig;              // ȫ�����ö���...
	TextLookup                 m_textLookup;                // ���ַ������...
	string                     m_strWebCenter;              // ����������վ��ַ...
	string                     m_strWebClass;				// �����ƽ�����վ��ַ...
	bool                       m_bIsDebugMode;              // �Ƿ��ǵ���ģʽ => ���ص����Է�����...
};

inline CScreenApp *App() { return static_cast<CScreenApp*>(qApp); }
inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);
bool WindowPositionValid(QRect rect);
