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
#include "measurement_log.h"
#include "webserver_dashboard/web_server.h"

// OLED: 72x40 I2C (GPIO6=SCL, GPIO5=SDA)
// (U8g2 pins here override the default Wire pins)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
				    /* clock=*/6, /* data=*/5);

// Pin mapping (on perfboard)
static constexpr uint8_t PIN_TRIG = 2;
static constexpr uint8_t PIN_ECHO = 0;

static constexpr uint32_t PULSE_TIMEOUT_US = 30000; // 30ms ~ 5m max distance
static UltrasonicSensor g_ultrasonic(PIN_TRIG, PIN_ECHO, PULSE_TIMEOUT_US);

static MeasurementLog g_store;

static constexpr uint32_t PERIODIC_INTERVAL_MS = 2UL * 60UL * 60UL * 1000UL; // 2 hour
static constexpr uint32_t DEBUG_INTERVAL_MS = 250UL;

static constexpr const char *AP_SSID = "TankMonitor";
static constexpr const char *AP_PASSWORD = "tankmonitor";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

static RunMode g_mode = RunMode::PERIODIC;
static uint32_t g_measurementIntervalMs = PERIODIC_INTERVAL_MS;
static uint32_t g_lastMeasurementAtMs = 0;
static MeasureResult g_lastMeasurement = {MeasureStatus::ERROR, 0.0f, 0};

void setup()
{
	g_mode = detectRunModeByDoubleReset();
	g_measurementIntervalMs =
		(g_mode == RunMode::DEBUG_CONTINUOUS) ? DEBUG_INTERVAL_MS : PERIODIC_INTERVAL_MS;

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

	if (!g_store.begin())
		Serial.println("MeasurementLog: begin() failed");

	if (g_mode == RunMode::DEBUG_CONTINUOUS)
		Serial.println("Mode: DEBUG (continuous). Double-reset detected.");
	else
		Serial.println("Mode: PERIODIC (slow updates).");

	webServerBegin(AP_SSID, AP_PASSWORD, AP_IP, AP_GATEWAY, AP_SUBNET, g_store);

	// Trigger the first measurement immediately after boot
	g_lastMeasurementAtMs = millis() - g_measurementIntervalMs;
}

void loop()
{
	webServerLoop();

	const uint32_t nowMs = millis();

	if (nowMs - g_lastMeasurementAtMs >= g_measurementIntervalMs)
	{
		digitalWrite(8, !digitalRead(8));

		// Take 9 samples, require at least 5 valid ones to form a robust median
		g_lastMeasurement = g_ultrasonic.measureMedian(9, 5);
		// Store timestamp after measurement so debug mode still waits 250ms between runs
		g_lastMeasurementAtMs = millis();

		// Persist valid sample in periodic mode
		if (g_mode == RunMode::PERIODIC && g_lastMeasurement.status == MeasureStatus::OK)
		{
			const uint32_t uptimeSeconds = millis() / 1000UL;
			if (!g_store.append(g_lastMeasurement.distanceCm, uptimeSeconds))
				Serial.println("MeasurementLog: append failed");
		}

		if (g_lastMeasurement.status == MeasureStatus::ERROR)
			Serial.printf("Distance: ERROR (valid: %u)\n", g_lastMeasurement.validSamples);
		else
			Serial.printf("Distance: %.1f cm (median of %u valid)\n",
				      g_lastMeasurement.distanceCm,
				      g_lastMeasurement.validSamples);
	}

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tf);

	if (g_mode == RunMode::DEBUG_CONTINUOUS)
		u8g2.drawStr(0, 8, "AJ-SR04M (debug):");
	else
		u8g2.drawStr(0, 8, "AJ-SR04M Dist:");

	switch (g_lastMeasurement.status)
	{
		case MeasureStatus::ERROR:
			u8g2.drawStr(0, 22, "Measurement");
			u8g2.drawStr(0, 32, "Failed");
			break;
		case MeasureStatus::OK:
		{
			char buf[16];
			// Large font for 72x40
			u8g2.setFont(u8g2_font_logisoso18_tn);
			snprintf(buf, sizeof(buf), "%.1f", g_lastMeasurement.distanceCm);
			u8g2.drawStr(0, 38, buf);

			u8g2.setFont(u8g2_font_5x7_tf);
			u8g2.drawStr(56, 38, "cm");
			break;
		}
	}

	u8g2.sendBuffer();
}