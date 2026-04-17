#pragma once

#include <Arduino.h>

#include "types.h"

// AJ-SR04M ultrasonic measurement wrapper.
//
// User Manual https://www.fabian.com.mt/viewer/42585/pdf.pdf
//
// Notes:
// - TRIG must be OUTPUT and is triggered by a 10us HIGH pulse
// - ECHO must be INPUT; its HIGH pulse width corresponds to sound roundtrip time
// - IMPORTANT: AJ-SR04M board might output 5V on ECHO; the ESP32-C3 GPIO is 3.3V max.
//   Therefore, using a resistor divider:
//	 IMPORTANT (Echo level shifting):
//   Rtop   = 1k  (ECHO -> node)
//   Rbottom= 2k  (node -> GND)  (two 1k in series)
//	 Divider math:
//	 Vout = Vin * Rbottom / (Rtop + Rbottom)
//        = 5.0V * 2k / (1k + 2k)
//        ≈ 3.33V  (safe for ESP32-C3 input)

class UltrasonicSensor
{
	public:
		UltrasonicSensor(uint8_t trigPin, uint8_t echoPin, uint32_t pulseTimeoutUs)
			: trigPin_(trigPin), echoPin_(echoPin), pulseTimeoutUs_(pulseTimeoutUs) { }

		void begin() const;

		MeasureResult measureOnce() const;
		MeasureResult measureMedian(uint8_t samples, uint8_t minValidRequired) const;

	private:
		uint8_t trigPin_;
		uint8_t echoPin_;
		uint32_t pulseTimeoutUs_;
};
