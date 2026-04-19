#pragma once

#include "types.h"

// double-reset detection across EN/RESET button presses
// RTC memory seems to get cleared on this (C3) board so using NVS
// Behavior:
// - First reset/boot: write a flag to NVS and enters PERIODIC
// - Second reset: reset the flag and enters DEBUG_CONTINUOUS
// effectively becomes an every-second-reset toggle
RunMode detectRunModeByDoubleReset();
