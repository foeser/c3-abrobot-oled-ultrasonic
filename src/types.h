#pragma once

#include <Arduino.h>

// double-reset detection across EN/RESET button presses
// RTC memory seems to get cleared on this (C3) board so using NVS
// Behavior:
// - First reset/boot: write a flag to NVS and enters PERIODIC
// - Second reset: reset the flag and enters DEBUG_CONTINUOUS
// effectively becomes an every-second-reset toggle
enum class RunMode : uint8_t
{
	DEBUG_CONTINUOUS = 0, // fast continuous measurements
	PERIODIC = 1 // slow periodic measurements
};

enum class MeasureStatus : uint8_t
{
	OK = 0,
	ERROR = 1
};

struct MeasureResult
{
	MeasureStatus status;
	float distanceCm;
	uint8_t validSamples; // how many samples were OK (used for median)
};
