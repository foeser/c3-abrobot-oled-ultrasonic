/**
 * ESP32-C3 + 0.42" OLED (72x40) + Ultrasonic distance (AJ-SR04M)
 *
 * Display
 * - 0.42" OLED modules for ESP32-C3 commonly use SH1106 controllers.
 * - This module resolution is 72x40.
 * - U8g2 is used for SH1106/72x40 support.
 *
 * Wiring (based on https://emalliab.wordpress.com/2025/02/12/esp32-c3-0-42-oled/):
 * - OLED I2C: SDA=GPIO5, SCL=GPIO6
 * - Ultrasonic: TRIG=GPIO2 (OUT), ECHO=GPIO0 (IN)
 *
 * AJ-SR04M
 * User Manual https://www.fabian.com.mt/viewer/42585/pdf.pdf
 *
 * IMPORTANT (Echo level shifting):
 * - This sensor may have 5V ECHO output on some boards.
 * - ESP32-C3 GPIO inputs are 3.3V max.
 * - A level shifter / resistor divider is required on ECHO.
 *   Rtop   = 1k  (ECHO -> node)
 *   Rbottom= 2k  (node -> GND)  (two 1k in series)
 * Divider math:
 *   Vout = Vin * Rbottom / (Rtop + Rbottom)
 *        = 5.0V * 2k / (1k + 2k)
 *        ≈ 3.33V  (safe for ESP32-C3 input)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>

// OLED: 72x40 I2C (GPIO6=SCL, GPIO5=SDA)
// (U8g2 pins here override the default Wire pins)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
				    /* clock=*/6, /* data=*/5);

// Pin mapping (on perfboard)
static constexpr uint8_t PIN_TRIG = 2;
static constexpr uint8_t PIN_ECHO = 0;

static constexpr uint32_t PULSE_TIMEOUT_US = 30000; // 30ms ~ 5m max distance

// double-reset detection across EN/RESET button presses
// RTC memory seems to get cleared on this (C3) board so using NVS
// Behavior:
// - First reset/boot: write a flag to NVS and enters PERIODIC
// - Second reset: reset the flag and enters DEBUG_CONTINUOUS
// effectively becomes an every-second-reset toggle
enum class RunMode : uint8_t
{
	DEBUG_CONTINUOUS = 0,
	PERIODIC = 1
};

static constexpr const char *NVS_NS = "runmode";
static constexpr const char *NVS_KEY_DEBUG_NEXT_BOOT = "debug_next_boot";

static Preferences g_prefs;

static RunMode detectRunModeByDoubleReset()
{
	g_prefs.begin(NVS_NS, false);

	const bool debug_on_next_boot = g_prefs.getBool(NVS_KEY_DEBUG_NEXT_BOOT, false);
	if (debug_on_next_boot)
	{
		g_prefs.putBool(NVS_KEY_DEBUG_NEXT_BOOT, false);
		g_prefs.end();
		return RunMode::DEBUG_CONTINUOUS;
	}

	// enable debug on the next boot
	g_prefs.putBool(NVS_KEY_DEBUG_NEXT_BOOT, true);
	g_prefs.end();

	return RunMode::PERIODIC;
}

static RunMode g_mode = RunMode::PERIODIC;

enum class MeasureStatus
{
	OK,
	ERROR // TIMEOUT and unstable distance returned
};

struct MeasureResult
{
	MeasureStatus status;
	float distanceCm;
	uint32_t rawDurationUs;
	uint8_t validSamples; // how many samples were OK (used for median)
};

static MeasureResult measureDistanceCmOnce();
static MeasureResult measureDistanceCmMedian(uint8_t samples, uint8_t minValidRequired);

void setup()
{
	g_mode = detectRunModeByDoubleReset();

	Serial.begin(115200);
	delay(3000); // allow Serial Monitor attach, but still boot standalone

	pinMode(PIN_TRIG, OUTPUT);
	pinMode(PIN_ECHO, INPUT);
	digitalWrite(PIN_TRIG, LOW);

	// Onboard LED (common on ESP32-C3 DevKitM-1): GPIO8
	pinMode(8, OUTPUT);
	digitalWrite(8, LOW);

	u8g2.begin();
	u8g2.setContrast(255);

	Serial.println("ESP32-C3 ultrasonic + U8g2 OLED starting...");
	Serial.printf("Reset reason (CPU0): %d\n", static_cast<int>(esp_reset_reason()));

	if (g_mode == RunMode::DEBUG_CONTINUOUS)
		Serial.println("Mode: DEBUG (continuous). Double-reset detected.");
	else
		Serial.println("Mode: PERIODIC (slow updates).");
}

void loop()
{
	digitalWrite(8, !digitalRead(8));

	// Take 9 samples, require at least 5 valid ones to form a robust median
	const MeasureResult res = measureDistanceCmMedian(9, 5);

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tf);

	if (g_mode == RunMode::DEBUG_CONTINUOUS)
		u8g2.drawStr(0, 8, "AJ-SR04M (debug):");
	else
		u8g2.drawStr(0, 8, "AJ-SR04M Dist:");

	switch (res.status)
	{
	case MeasureStatus::ERROR:
		u8g2.drawStr(0, 22, "Measurement");
		u8g2.drawStr(0, 32, "Failed");
		Serial.printf("Distance: ERROR (valid: %u)\n", res.validSamples);
		break;
	case MeasureStatus::OK:
	{
		char buf[16];
		// Large font for 72x40
		u8g2.setFont(u8g2_font_logisoso18_tn);
		snprintf(buf, sizeof(buf), "%.1f", res.distanceCm);
		u8g2.drawStr(0, 38, buf);

		u8g2.setFont(u8g2_font_5x7_tf);
		u8g2.drawStr(56, 38, "cm");

		Serial.printf("Distance: %.1f cm (median of %u valid)\n", res.distanceCm, res.validSamples);
		break;
	}
	}

	u8g2.sendBuffer();

	if (g_mode == RunMode::DEBUG_CONTINUOUS)
		delay(250);
	else
		delay(60UL * 60UL * 1000UL); // 1 hour
}

static MeasureResult measureDistanceCmOnce()
{
	// Trigger pulse: LOW 2us, HIGH 10us, then LOW
	digitalWrite(PIN_TRIG, LOW);
	delayMicroseconds(2);
	digitalWrite(PIN_TRIG, HIGH);
	delayMicroseconds(10);
	digitalWrite(PIN_TRIG, LOW);

	// pulseIn(): measures ECHO HIGH time in microseconds = round-trip time of sound
	const uint32_t duration = pulseIn(PIN_ECHO, HIGH, PULSE_TIMEOUT_US);
	if (duration == 0)
	{
		return {MeasureStatus::ERROR, 0.0f, 0, 0};
	}

	// Distance calculation:
	//   Speed of sound: 343 m/s @ ~20°C = 0.0343 cm/us
	//   Total distance = duration (us) * speed (cm/us)
	//   One-way distance = Total distance / 2 (because the pulse travels to the object and back)
	const float distanceCm = (static_cast<float>(duration) * 0.0343f) / 2.0f;

	return {MeasureStatus::OK, distanceCm, duration, 0};
}

// Helper for sorting (Bubble sort, suitable for very small arrays <= 10)
static void sortFloatArray(float *arr, uint8_t n)
{
	for (uint8_t i = 0; i < n - 1; i++)
	{
		for (uint8_t j = 0; j < n - i - 1; j++)
		{
			if (arr[j] > arr[j + 1])
			{
				float temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}

// Median of N samples with minimum valid requirement:
// - collect valid (OK) readings
// - delay(30) between samples to avoid interference
// - require at least 'minValidRequired' valid samples, else ERROR
// - sort and pick middle value to ignore spikes/outliers
// - detect large spread (ignoring extremes) to report ERROR
static MeasureResult measureDistanceCmMedian(uint8_t samples, uint8_t minValidRequired)
{
	if (samples == 0 || minValidRequired == 0)
		return {MeasureStatus::ERROR, 0.0f, 0, 0};

	// We'll collect up to 'samples' valid readings.
	// We use a fixed-size stack array because dynamic allocation (like std::vector)
	// on embedded systems can lead to heap fragmentation.
	const uint8_t MAX_SAMPLES = 15;
	float validReadings[MAX_SAMPLES];
	uint8_t validCount = 0;

	uint8_t n = (samples <= MAX_SAMPLES) ? samples : MAX_SAMPLES;

	for (uint8_t i = 0; i < n; i++)
	{
		MeasureResult r = measureDistanceCmOnce();
		if (r.status == MeasureStatus::OK)
		{
			validReadings[validCount++] = r.distanceCm;
		}
		delay(30);
	}

	// Require a minimum number of valid readings to trust the median
	if (validCount < minValidRequired)
	{
		return {MeasureStatus::ERROR, 0.0f, 0, validCount};
	}

	sortFloatArray(validReadings, validCount);

	float median = 0.0f;
	if (validCount % 2 == 0)
	{
		// Even number of samples: median is the average of the two middle elements
		median = (validReadings[(validCount / 2) - 1] + validReadings[validCount / 2]) / 2.0f;
	}
	else
	{
		// Odd number of samples: median is the exact middle element
		median = validReadings[validCount / 2];
	}

	// Robust spread check:
	// The array is sorted from smallest [0] to largest [validCount-1].
	// If we have enough samples (e.g., >= 5), we calculate the "spread"
	// not by subtracting the absolute minimum from the absolute maximum,
	// but by checking the difference between the 2nd smallest [1] and 2nd largest [validCount-2].
	// Why? Ultrasonic sensors frequently report a single rogue spike (too far or too close)
	// due to a stray reflection or missed echo. By discarding the outermost points,
	// we ensure that an otherwise stable tight cluster of 3+ readings isn't thrown
	// out as "unstable" just because one reading bounced off a side wall.
	float spread = 0.0f;
	if (validCount >= 5)
	{
		spread = validReadings[validCount - 2] - validReadings[1];
	}
	else
	{
		spread = validReadings[validCount - 1] - validReadings[0];
	}

	// Heuristic: > 5cm spread across the core valid samples means we have a bouncing signal
	if (spread > 5.0f)
	{
		return {MeasureStatus::ERROR, median, 0, validCount};
	}

	return {MeasureStatus::OK, median, 0, validCount};
}
