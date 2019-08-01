
#pragma once

#include "screen-app.h"

#define WIN_MANIFEST_URL    DEF_WEB_CLASS "/update_studio/screen/manifest.json"
#define WIN_DXSETUP_URL     DEF_WEB_CLASS "/update_studio/dxwebsetup.exe"
#define WIN_UPDATER_URL     DEF_WEB_CLASS "/update_studio/updater.exe"
#define APP_DXSETUP_PATH    "obs-teacher\\updates\\dxwebsetup.exe"
#define APP_MANIFEST_PATH   "obs-screen\\updates\\manifest.json"
#define APP_UPDATER_PATH    "obs-screen\\updates\\updater.exe"
#define APP_NAME            L"screen"

void doCloseMainWindow();
