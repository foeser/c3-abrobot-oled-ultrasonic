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

## Pictures

Some pictures of the project during development:

<table>
<tr>
    <td>
      <figure>
        <img src="docs/img/breadboard_esp32.jpg" alt="First prototype on breadboard" width="600" />
        <figcaption><sub>First prototype on Breadboard with classical ESP32-D0WD</sub></figcaption>
</figure>
    </td>
    <td>
      <figure>
        <img src="docs/img/breadboard_esp32_c3_oled.jpg" alt="Small prototype with ESP32‑C3 and OLED on breadboard" width="400" />
        <figcaption><sub>Small prototype with ESP32‑C3 and OLED on breadboard</sub></figcaption>
</figure>
    </td>
  </tr>
  <tr>
    <td>
      <figure>
        <img src="docs/img/perfboard_front.jpg" alt="Prototype soldered on Perfboard (front)" width="600" />
        <figcaption><sub>First time soldering experience :) Prototype soldered on perfboard. AJ‑SR04M sensor board is glued.</sub></figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <img src="docs/img/perfboard_front_2.jpg" alt="Prototype soldered on Perfboard (front)" width="400" />        
      </figure>
    </td>
  </tr>
  <tr>
    <td>
      <figure>
        <img src="docs/img/perfboard_back.jpg" alt="Back view of perfboard" width="600" />
        <figcaption><sub>Back view of perfboard with plug for sensor cable</sub></figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <img src="docs/img/perfboard_front_running.jpg" alt="Running prototype on perfboard" width="400" />        
      </figure>
    </td>
  </tr>
</table>
