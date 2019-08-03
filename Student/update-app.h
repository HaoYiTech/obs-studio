
#pragma once

#include "student-app.h"

#define WIN_MANIFEST_URL    DEF_WEB_CLASS "/update_studio/student/manifest.json"
#define WIN_DXSETUP_URL     DEF_WEB_CLASS "/update_studio/dxwebsetup.exe"
#define WIN_UPDATER_URL     DEF_WEB_CLASS "/update_studio/updater.exe"
#define APP_DXSETUP_PATH    "obs-student\\updates\\dxwebsetup.exe"
#define APP_MANIFEST_PATH   "obs-student\\updates\\manifest.json"
#define APP_UPDATER_PATH    "obs-student\\updates\\updater.exe"
#define APP_NAME            L"student"

void doCloseMainWindow();
