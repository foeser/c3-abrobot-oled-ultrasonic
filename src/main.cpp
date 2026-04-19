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
 * See `src/ultrasonic.h` for details.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "types.h"
#include "run_mode.h"
#include "ultrasonic.h"

// OLED: 72x40 I2C (GPIO6=SCL, GPIO5=SDA)
// (U8g2 pins here override the default Wire pins)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
				    /* clock=*/6, /* data=*/5);

// Pin mapping (on perfboard)
static constexpr uint8_t PIN_TRIG = 2;
static constexpr uint8_t PIN_ECHO = 0;

static constexpr uint32_t PULSE_TIMEOUT_US = 30000; // 30ms ~ 5m max distance

static UltrasonicSensor g_ultrasonic(PIN_TRIG, PIN_ECHO, PULSE_TIMEOUT_US);

static RunMode g_mode = RunMode::PERIODIC;

void setup()
{
	g_mode = detectRunModeByDoubleReset();

	Serial.begin(115200);
	delay(3000); // allow Serial Monitor attach, but still boot standalone

	g_ultrasonic.begin();

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
	const MeasureResult res = g_ultrasonic.measureMedian(9, 5);

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
