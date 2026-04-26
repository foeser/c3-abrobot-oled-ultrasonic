#pragma once

#include <Arduino.h>

#include "types.h"

// Measurement persistence.
//
// Storage design: LittleFS + CSV log file
//
// CSV format:
//   uptimeSeconds,distanceCm\n
// Example:
//   98765,79.00
//
// Policy:
// - Append one line per periodic measurement
// - If free space drops below a safety threshold, delete the log file and start fresh
class MeasurementLog
{
public:
	// Mount LittleFS and ensure the log file exists
	bool begin();

	// Delete the log file
	bool clear();

	// Append one sample line to the CSV
	bool append(float distanceCm, uint32_t uptimeSeconds);

	// Number of valid samples currently known in the log
	size_t sampleCount() const;

	// Return all valid samples as a JSON array:
	// [{"uptimeSeconds":123,"distanceCm":45.6}, ...]
	String readJson() const;

	size_t totalBytes() const;
	size_t usedBytes() const;
	size_t freeBytes() const;

private:
	bool ensureLogFileExists();
	bool rotateLogOnLowFreeSpace();
	bool rebuildSampleCount();

	size_t sampleCount_ = 0;
};
