
#pragma once

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

#include "window-main.hpp"
#include "window-login-main.h"

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

class QListWidget;
class CTrackerSession;
class CStorageSession;
class OBSApp : public QApplication {
	Q_OBJECT

private:
	int                            m_nFastTimer;
	int                            m_nActiveTimer;
	std::string                    m_strUTF8Data;
	std::string                    locale;
	std::string	                   theme;
	ConfigFile                     globalConfig;
	TextLookup                     textLookup;
	OBSContext                     obsContext;
	QPointer<OBSMainWindow>        mainWindow;
	QPointer<LoginWindow>          loginWindow;
	QPointer<CTrackerSession>      m_TrackerSession;
	QPointer<CStorageSession>      m_StorageSession;

	profiler_name_store_t          *profilerNameStore = nullptr;

	os_inhibit_t                   *sleepInhibitor = nullptr;
	int                            sleepInhibitRefs = 0;

	std::deque<obs_frontend_translate_ui_cb> translatorHooks;

	bool InitGlobalConfig();
	bool InitGlobalConfigDefaults();
	bool InitLocale();
	bool InitTheme();
public slots:
	void doLoginSuccess();
public:
	OBSApp(int &argc, char **argv, profiler_name_store_t *store);
	~OBSApp();

	void AppInit();
	bool OBSInit();
	
	void doLoginInit();
	void doLogoutEvent();
	void doPostCurl(char *pData, size_t nSize);
	
	void doCameraGetOnLineList(QListWidget * lpListView, int inSelCameraID);
	void doCameraGetRtmpUrl(int nCameraID, obs_data_t * lpSettings);
	void doCameraVerifyEvent(obs_data_t * lpSettings);

	void doCheckTracker();
	void doCheckStorage();
	void doCheckFDFS();
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

	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;

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
	OBSMainWindow *window = reinterpret_cast<OBSMainWindow*>(
		App()->GetMainWindow());
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
