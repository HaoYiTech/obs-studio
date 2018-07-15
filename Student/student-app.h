#pragma once

#include <QApplication>
#include <QPointer>

#include <obs.hpp>
#include <util/lexer.h>
#include <util/util.hpp>
#include <util/platform.h>
#include "window-login-main.h"

class CStudentApp : public QApplication {
	Q_OBJECT
public:
	CStudentApp(int &argc, char **argv);
	~CStudentApp();
public:
	void doLoginInit();
	void doLogoutEvent();
public slots:
	void doLoginSuccess();
public:
	inline const char *GetString(const char *lookupVal) const
	{
		return m_textLookup.GetString(lookupVal);
	}
private:
	QPointer<LoginWindow>      m_loginWindow;
	TextLookup                 m_textLookup;
};

inline CStudentApp *App() { return static_cast<CStudentApp*>(qApp); }
inline const char *Str(const char *lookup) { return App()->GetString(lookup); }
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))
