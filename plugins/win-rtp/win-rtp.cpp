
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-rtp", "en-US")

void RegisterWinRtpSource();
void RegisterWinRtpOutput();

bool obs_module_load(void)
{
	RegisterWinRtpSource();
	RegisterWinRtpOutput();
	return true;
}