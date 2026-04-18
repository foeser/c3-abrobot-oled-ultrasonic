# ESP32‑C3 tank level monitor (AJ‑SR04M ultrasonic)

Small ESP32‑C3 project that measures distance with an **AJ‑SR04M ultrasonic sensor** and shows the latest reading on the built‑in 0.42″ OLED of the **ABRobot ESP32‑C3**.

Intended as a simple building block for tank monitoring (distance to liquid surface).

## Status
- Working: distance measurement + OLED output
- Two run modes:
  - **Periodic** (low update rate)
  - **Debug** (fast updates)

## Documentation
- Ultrasonic measurement/filtering notes: [`docs/ultrasonic-measurement.md`](docs/ultrasonic-measurement.md)

## Hardware
- ABRobot ESP32‑C3 0.42″ OLED (ESP32‑C3 DevKitM‑1)
- AJ‑SR04M ultrasonic sensor

## Build / Flash
Built with **PlatformIO**.
