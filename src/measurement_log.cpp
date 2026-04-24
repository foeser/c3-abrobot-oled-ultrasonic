#include "measurement_log.h"

#include <FS.h>
#include <LittleFS.h>
#include <stdio.h>

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

	if (!ensureLogFileExists())
		return false;

	return rebuildSampleCount();
}

bool MeasurementLog::clear()
{
	if (LittleFS.exists(LOG_PATH) && !LittleFS.remove(LOG_PATH))
	{
		Serial.println("MeasurementLog: failed to remove log file");
		return false;
	}

	if (!ensureLogFileExists())
		return false;

	sampleCount_ = 0;
	return true;
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
		Serial.println("MeasurementLog: failed to open log for append");
		return false;
	}

	f.printf("%lu,%.2f\n", static_cast<unsigned long>(uptimeSeconds), distanceCm);
	const bool ok = (f.getWriteError() == 0);
	f.close();

	if (ok)
		sampleCount_++;

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

size_t MeasurementLog::sampleCount() const
{
	return sampleCount_;
}

String MeasurementLog::readJson() const
{
	String json;
	json.reserve(sampleCount_ * 40 + 2);
	json += "[";

	if (!LittleFS.exists(LOG_PATH))
	{
		json += "]";
		return json;
	}

	File f = LittleFS.open(LOG_PATH, "r");
	if (!f)
	{
		Serial.println("MeasurementLog: failed to open log for JSON read");
		json += "]";
		return json;
	}

	bool first = true;
	while (f.available())
	{
		const String line = f.readStringUntil('\n');
		if (line.length() == 0)
			continue;

		unsigned long uptimeSeconds = 0;
		float distanceCm = 0.0f;
		if (sscanf(line.c_str(), "%lu,%f", &uptimeSeconds, &distanceCm) != 2)
			continue;

		if (!first)
			json += ",";

		first = false;
		json += "{\"uptimeSeconds\":";
		json += String(uptimeSeconds);
		json += ",\"distanceCm\":";
		json += String(distanceCm, 2);
		json += "}";
	}

	f.close();
	json += "]";
	return json;
}

bool MeasurementLog::ensureLogFileExists()
{
	if (LittleFS.exists(LOG_PATH))
		return true;

	File w = LittleFS.open(LOG_PATH, "w");
	if (!w)
	{
		Serial.println("MeasurementLog: failed to create log file");
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

	Serial.println("MeasurementLog: low LittleFS space; resetting log file");
	return clear();
}

bool MeasurementLog::rebuildSampleCount()
{
	sampleCount_ = 0;

	File f = LittleFS.open(LOG_PATH, "r");
	if (!f)
	{
		Serial.println("MeasurementLog: failed to open log for counting");
		return false;
	}

	while (f.available())
	{
		const String line = f.readStringUntil('\n');
		if (line.length() == 0)
			continue;

		unsigned long uptimeSeconds = 0;
		float distanceCm = 0.0f;
		if (sscanf(line.c_str(), "%lu,%f", &uptimeSeconds, &distanceCm) == 2)
			sampleCount_++;
	}

	f.close();
	return true;
}