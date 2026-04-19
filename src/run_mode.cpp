#include "run_mode.h"

#include <Preferences.h>

namespace {
	static constexpr const char *NVS_NS = "runmode";
	static constexpr const char *NVS_KEY_DEBUG_NEXT_BOOT = "debug_next_boot";
}

RunMode detectRunModeByDoubleReset()
{
	Preferences prefs;
	prefs.begin(NVS_NS, false);

	const bool debug_on_next_boot = prefs.getBool(NVS_KEY_DEBUG_NEXT_BOOT, false);
	if (debug_on_next_boot)
	{
		prefs.putBool(NVS_KEY_DEBUG_NEXT_BOOT, false);
		prefs.end();
		return RunMode::DEBUG_CONTINUOUS;
	}

	// enable debug on the next boot
	prefs.putBool(NVS_KEY_DEBUG_NEXT_BOOT, true);
	prefs.end();

	return RunMode::PERIODIC;
}
