#pragma once

#include <Arduino.h>

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
