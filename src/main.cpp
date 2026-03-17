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

// OLED: 72x40 I2C (GPIO6=SCL, GPIO5=SDA)
// (U8g2 pins here override the default Wire pins)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
				    /* clock=*/6, /* data=*/5);

// Pin mapping (on perfboard)
static constexpr uint8_t PIN_TRIG = 2;
static constexpr uint8_t PIN_ECHO = 0;

static constexpr uint32_t PULSE_TIMEOUT_US = 30000; // 30ms ~ 5m max distance

static float measureDistanceCmOnce();
static float measureDistanceCmAverage(uint8_t samples);

void setup()
{
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
}

void loop()
{
	digitalWrite(8, !digitalRead(8));

	const float distCm = measureDistanceCmAverage(5);

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tf);
	u8g2.drawStr(0, 8, "AJ-SR04M Dist:");

	if (distCm < 0)
	{
		// No echo (timeout)
		u8g2.drawStr(0, 22, "No echo");
		u8g2.drawStr(0, 32, "(check wiring)");
		Serial.println("Distance: no echo / timeout");
	}
	else
	{
		char buf[16];
		// Large font for 72x40
		u8g2.setFont(u8g2_font_logisoso18_tn);
		snprintf(buf, sizeof(buf), "%.1f", distCm);
		u8g2.drawStr(0, 38, buf);

		u8g2.setFont(u8g2_font_5x7_tf);
		u8g2.drawStr(56, 38, "cm");

		Serial.printf("Distance: %.1f cm\n", distCm);
	}

	u8g2.sendBuffer();
	delay(250);
}

static float measureDistanceCmOnce()
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
		return -1.0f; // timeout / no echo
	}

	// Distance calculation:
	//   Speed of sound: 343 m/s @ ~20°C = 0.0343 cm/us
	//   Total distance = duration (us) * speed (cm/us)
	//   One-way distance = Total distance / 2 (because the pulse travels to the object and back)
	return (static_cast<float>(duration) * 0.0343f) / 2.0f;
}

// Average N samples:
// - accumulate only d >= 0 (valid) readings
// - delay(30) between samples
// - return -1 if no valid readings
static float measureDistanceCmAverage(uint8_t samples)
{
	float total = 0.0f;
	uint8_t valid = 0;

	for (uint8_t i = 0; i < samples; i++)
	{
		const float d = measureDistanceCmOnce();
		if (d >= 0)
		{
			total += d;
			valid++;
		}
		delay(30);
	}

	if (valid == 0)
	{
		return -1.0f;
	}
	return total / static_cast<float>(valid);
}
