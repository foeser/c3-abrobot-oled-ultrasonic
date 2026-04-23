#include "measurement_log.h"

#include <FS.h>
#include <LittleFS.h>

namespace {
	static constexpr const char *LOG_PATH = "/meas.csv";
	static constexpr size_t MIN_FREE_BYTES = 8U * 1024U; // 8kb, if less we rotate
}

bool MeasurementLog::begin()
{
	// Try to mount without formatting first (avoid accidental data loss)
	if (!LittleFS.begin(false))
	{
		Serial.println("LittleFS mount failed; formatting...");
		if (!LittleFS.begin(true))
		{
			Serial.println("LittleFS mount failed even after format.");
			return false;
		}
	}

	Serial.printf("LittleFS: total=%u used=%u free=%u\n",
		      static_cast<unsigned>(totalBytes()),
		      static_cast<unsigned>(usedBytes()),
		      static_cast<unsigned>(freeBytes()));

	return ensureLogFileExists();
}

bool MeasurementLog::clear()
{
	if (LittleFS.exists(LOG_PATH) && !LittleFS.remove(LOG_PATH))
	{
		Serial.println("MeasurementStore: failed to remove log file");
		return false;
	}

	return ensureLogFileExists();
}

bool MeasurementLog::append(const float distanceCm, const uint32_t uptimeSeconds)
{
	if (!maybeRotateByReset())
		return false;

	if (!ensureLogFileExists())
		return false;

	File f = LittleFS.open(LOG_PATH, "a");
	if (!f)
	{
		Serial.println("MeasurementStore: failed to open log for append");
		return false;
	}

	f.printf("%lu,%.2f\n", static_cast<unsigned long>(uptimeSeconds), distanceCm);
	const bool ok = (f.getWriteError() == 0);
	f.close();
	return ok;
}

size_t MeasurementLog::totalBytes() const
{
	return LittleFS.totalBytes();
}

size_t MeasurementLog::usedBytes() const
{
	return LittleFS.usedBytes();
}

size_t MeasurementLog::freeBytes() const
{
	const size_t total = totalBytes();
	const size_t used = usedBytes();
	return (used >= total) ? 0U : (total - used);
}

bool MeasurementLog::ensureLogFileExists()
{
	if (LittleFS.exists(LOG_PATH))
		return true;

	File w = LittleFS.open(LOG_PATH, "w");
	if (!w)
	{
		Serial.println("MeasurementStore: failed to create log file");
		return false;
	}
	// No header; empty file is valid.
	w.close();
	return true;
}

bool MeasurementLog::maybeRotateByReset()
{
	if (freeBytes() >= MIN_FREE_BYTES)
		return true;

	Serial.println("MeasurementStore: low LittleFS space; resetting log file");
	return clear();
}
