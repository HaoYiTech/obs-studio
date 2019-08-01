
#pragma once

#include "obs-app.hpp"

#define WIN_MANIFEST_URL    DEF_WEB_CLASS "/update_studio/teacher/manifest.json"
#define WIN_DXSETUP_URL     DEF_WEB_CLASS "/update_studio/dxwebsetup.exe"
#define WIN_UPDATER_URL     DEF_WEB_CLASS "/update_studio/updater.exe"
#define APP_DXSETUP_PATH    "obs-teacher\\updates\\dxwebsetup.exe"
#define APP_MANIFEST_PATH   "obs-teacher\\updates\\manifest.json"
#define APP_UPDATER_PATH    "obs-teacher\\updates\\updater.exe"
#define APP_NAME            L"teacher"

void doCloseMainWindow();
