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

#include "window-login-main.h"
#include "window-student-main.h"

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

class CStudentApp : public QApplication {
	Q_OBJECT
public:
	CStudentApp(int &argc, char **argv);
	~CStudentApp();
public:
	void AppInit();
public:
	void doLoginInit();
	void doLogoutEvent();
public slots:
	void doLoginSuccess(string & strRoomID);
public:
	bool TranslateString(const char *lookupVal, const char **out) const;
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
private:
	bool	InitLocale();
	bool	InitGlobalConfig();
	bool	InitGlobalConfigDefaults();
private:
	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	QPointer<StudentWindow>    m_studentWindow;
	QPointer<LoginWindow>      m_loginWindow;
	ConfigFile                 m_globalConfig;
	TextLookup                 m_textLookup;
};

inline CStudentApp *App() { return static_cast<CStudentApp*>(qApp); }
inline config_t *GetGlobalConfig() { return App()->GlobalConfig(); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

bool WindowPositionValid(QRect rect);
