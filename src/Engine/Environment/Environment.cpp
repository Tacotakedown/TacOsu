#include "Environment.h"

#include "ConVar/ConVar.h"

ConVar _debug_env("debug_env", false);
ConVar* Environment::debug_env = &_debug_env;

Environment::Environment() {
	m_bFullscreenWindowedBorderless = false;
}

void Environment::setFullscreenWindowedBorderless(bool fullscreenWindowdBorderless) {
	m_bFullscreenWindowedBorderless = fullscreenWindowdBorderless;

	if (env->isFullscreen()) {
		env->disableFullscreen();
		env->enableFullscreen();
	}
}

void _fullscreen_windowed_borderless(UString oldValue, UString newValue) {
	env->setFullscreenWindowedBorderless(newValue.toFloat() > 0.0f);
}

void _monitor(UString oldValue, UString newValue) {
	env->setMonitor(newValue.toInt());
}

ConVar _fullscreen_windowed_borderless_("fullscreen_windowed_borderless", false, _fullscreen_windowed_borderless);
ConVar _monitor_("monitor", 0, "monitor/display device to switch to, 0 = primary monitor", _monitor);