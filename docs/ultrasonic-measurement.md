# Ultrasonic Measurement (AJ-SR04M)

This document explains the logic behind the distance measurement and filtering implemented in `src/main.cpp`.

## 1. Single Measurement (`measureDistanceCmOnce`)

The sensor measures distance by sending an ultrasonic pulse and timing how long it takes for the echo to return.

- Arduino's `pulseIn(pin, HIGH, timeout)` blocks and waits for the ECHO pin to go HIGH, then measures how long it stays HIGH in microseconds.
- **Timeout:** If no pulse is received within the specified timeout, `pulseIn()` returns `0`. This is our primary indicator of "No echo / Too far".
- **Distance Calculation:** The time is converted to distance using the speed of sound (~343 m/s or 0.0343 cm/µs). We multiply the duration by the speed and divide by 2 (since the sound traveled to the object and back).

## 2. Filtering with Median (`measureDistanceCmMedian`)

Ultrasonic sensors can occasionally produce wildly inaccurate readings (spikes or outliers) due to random reflections, acoustic noise, or multipath interference in enclosed spaces like tanks. A simple average would be heavily skewed by a single bad reading.

Instead, we use a **robust median filter with a minimum quorum**:
1. Take a larger burst of readings (e.g., 9 samples).
2. Discard invalid readings (timeouts).
3. **Quorum Check**: Require a minimum number of valid readings (e.g., at least 5) to proceed. This prevents trusting a median built from just 1 or 2 stray echoes.
4. Sort the remaining valid readings in ascending order.
5. Pick the middle value.
6. **Robust Spread Check**: Ensure the core cluster of readings is tight, ignoring the absolute highest and lowest outliers.

### Step-by-Step Logic

#### a. Collection
We loop `samples` times. If a reading is valid (`OK`), we store it in the `validReadings` array and increment `validCount`. We delay 30ms between readings to prevent acoustic interference between pulses.

#### b. Error Handling (Quorum)
If `validCount < minValidRequired`, we immediately return `ERROR`. We don't have enough confidence in the data.

#### c. Sorting
We sort the `validReadings` array ascending using a simple Bubble Sort algorithm. While inefficient for large datasets, it is perfectly fine for arrays of < 15 elements.

#### d. Median Calculation (Odd vs Even)
Finding the middle element depends on how many valid readings we have:
- **Odd count (e.g., 5):** There is exactly one middle element at index `count / 2` (integer division: 5/2 = index 2, which is the 3rd element).
- **Even count (e.g., 4):** There are two middle elements at indices `count/2 - 1` and `count/2` (indices 1 and 2). The standard mathematical median is the average of these two values.

#### e. Robust Stability/Spread Check
In a noisy environment, taking the spread as `max - min` is too sensitive; a single rogue reading can invalidate 8 good readings.
Because the array is sorted, the absolute minimum is at `[0]` and the absolute maximum is at `[validCount - 1]`.
If we have enough samples (e.g., 5 or more), we calculate the spread by looking at the *inner* bounds: `validReadings[validCount - 2] - validReadings[1]`.
This effectively throws away the single highest and single lowest reading just for the sake of the stability check. If the remaining inner cluster has a spread > 5cm, we return `ERROR`.

## Worked Example

Imagine we ask for 9 samples, require 5 valid, and the sensor returns:
`[51.0, TIMEOUT, 50.5, 200.0, 51.2, 50.8, TIMEOUT, 51.5, 10.0]`

1. **Collection:**
   - Valid readings: `[51.0, 50.5, 200.0, 51.2, 50.8, 51.5, 10.0]`. `validCount` = 7.
2. **Quorum:**
   - `7 >= 5`, so we proceed.
3. **Sorting:** 
   - Sorted ascending: `[10.0, 50.5, 50.8, 51.0, 51.2, 51.5, 200.0]`
4. **Median:**
   - `validCount` is 7 (odd). Median index = 3.
   - `validReadings[3]` = **51.0 cm**.
5. **Robust Spread Check:**
   - Because `validCount >= 5`, we ignore `[0]` (10.0) and `[6]` (200.0).
   - Inner min = `[1]` = 50.5
   - Inner max = `[5]` = 51.5
   - Spread = 51.5 - 50.5 = 1.0 cm.
   - `1.0 <= 5.0`, so the measurement is stable. We return `OK` and `51.0 cm`.
