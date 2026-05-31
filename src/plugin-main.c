#include <obs-module.h>
#include <plugin-support.h>
#include "foobar2000-source.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	foobar2000_module_init();
	obs_register_source(&foobar2000_source_info);
	obs_log(LOG_INFO, "foobar2000 Now Playing plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	foobar2000_module_unload();
	obs_log(LOG_INFO, "foobar2000 Now Playing plugin unloaded");
}
