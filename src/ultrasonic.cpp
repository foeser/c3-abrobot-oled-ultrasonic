#include "ultrasonic.h"

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

void UltrasonicSensor::begin() const
{
	pinMode(trigPin_, OUTPUT);
	pinMode(echoPin_, INPUT);
	digitalWrite(trigPin_, LOW);
}

MeasureResult UltrasonicSensor::measureOnce() const
{
	// Trigger pulse: LOW 2us, HIGH 10us, then LOW
	digitalWrite(trigPin_, LOW);
	delayMicroseconds(2);
	digitalWrite(trigPin_, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin_, LOW);

	// pulseIn(): measures ECHO HIGH time in microseconds = round-trip time of sound
	const uint32_t duration = pulseIn(echoPin_, HIGH, pulseTimeoutUs_);
	if (duration == 0)
	{
		return {MeasureStatus::ERROR, 0.0f, 0};
	}

	// Distance calculation:
	//   Speed of sound: 343 m/s @ ~20°C = 0.0343 cm/us
	//   Total distance = duration (us) * speed (cm/us)
	//   One-way distance = Total distance / 2 (because the pulse travels to the object and back)
	const float distanceCm = (static_cast<float>(duration) * 0.0343f) / 2.0f;

	return {MeasureStatus::OK, distanceCm, 1};
}

// Median of N samples with a minimum valid requirement:
// - collect valid (OK) readings
// - delay(30) between samples to avoid interference
// - require at least 'minValidRequired' valid samples, else ERROR
// - sort and pick the middle value to ignore spikes/outliers
// - detect large spread (ignoring extremes) to report ERROR
MeasureResult UltrasonicSensor::measureMedian(uint8_t samples, uint8_t minValidRequired) const
{
	if (samples == 0 || minValidRequired == 0)
		return {MeasureStatus::ERROR, 0.0f, 0};

	// We'll collect up to 'samples' valid readings.
	// We use a fixed-size stack array because dynamic allocation (like std::vector)
	// on embedded systems can lead to heap fragmentation.
	const uint8_t MAX_SAMPLES = 15;
	float validReadings[MAX_SAMPLES];
	uint8_t validCount = 0;

	uint8_t n = (samples <= MAX_SAMPLES) ? samples : MAX_SAMPLES;

	for (uint8_t i = 0; i < n; i++)
	{
		MeasureResult r = measureOnce();
		if (r.status == MeasureStatus::OK)
		{
			validReadings[validCount++] = r.distanceCm;
		}
		delay(30);
	}

	// Require a minimum number of valid readings to trust the median
	if (validCount < minValidRequired)
	{
		return {MeasureStatus::ERROR, 0.0f, validCount};
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
		return {MeasureStatus::ERROR, median, validCount};
	}

	return {MeasureStatus::OK, median, validCount};
}
