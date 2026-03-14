/**
 * ESP32-C3 + 0.42" OLED (72x40) + Ultrasonic distance (AJ-SR04M)
 *
 * Display:
 * - The tiny 0.42" modules used with the ESP32-C3 are often SH1106-based and/or use a
 *   non-standard resolution like 72x40.
 * - The Adafruit_SSD1306 library is mainly targeted at SSD1306 128x64/128x32 displays.
 *   For these small SH1106/72x40 modules it can be unreliable or not directly supported
 *   without extra glue.
 * - U8g2 has broad controller + resolution support and your project already has a working
 *   constructor for this exact display, so we use U8g2 here.
 *
 * Wiring (based on https://emalliab.wordpress.com/2025/02/12/esp32-c3-0-42-oled/ and
 * your working `main_mini_c3abrobot.cpp`):
 * - OLED I2C: SDA=GPIO5, SCL=GPIO6
 * - Ultrasonic: TRIG=GPIO4 (OUT), ECHO=GPIO3 (IN)
 *
 * IMPORTANT (Echo level shifting):
 * Many ultrasonic modules output ~5V on ECHO. ESP32-C3 GPIOs are 3.3V only.
 * Use a resistor divider / level shifter.
 * Example divider you used before:
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

// OLED: 72x40 I2C, same constructor/pins as in main_mini_c3abrobot.cpp
// (U8g2 pins here override the default Wire pins)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
				    /* clock=*/6, /* data=*/5);

static constexpr uint8_t PIN_TRIG = 4;
static constexpr uint8_t PIN_ECHO = 3;

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

	// Optional: onboard LED on many ESP32-C3 DevKitM-1 setups is GPIO8
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
		// Timeout / no echo
		u8g2.drawStr(0, 22, "No echo");
		u8g2.drawStr(0, 32, "(check wiring)");
		Serial.println("Distance: no echo / timeout");
	}
	else
	{
		char buf[16];
		// Big readable font for the 72x40 screen
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

	const uint32_t duration = pulseIn(PIN_ECHO, HIGH, PULSE_TIMEOUT_US);
	if (duration == 0)
	{
		return -1.0f; // timeout / no echo
	}

	// Speed of sound: ~343 m/s => 0.0343 cm/us
	// Distance is half the round-trip
	return (static_cast<float>(duration) * 0.0343f) / 2.0f;
}

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
