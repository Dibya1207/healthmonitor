# IoT-Based Health Monitoring Device for Senior Citizens
<p align="center">
  <img width="450" alt="Device Circuit / Assembly" src="https://github.com/user-attachments/assets/cb0e3d62-a060-4931-ab57-c490aa2feec6" />
  <br/><br/>
  <img width="450" alt="Final Hardware Prototype" src="https://github.com/user-attachments/assets/2f5535a5-b578-46a2-a4d1-a4f79d830542" />
</p>



A compact wearable health-monitoring prototype engineered using the ESP32 microcontroller to track vital signs—including body temperature, heart rate, and blood oxygen saturation (SpO₂) in real time.

## Key Features

* **Real-Time Vital Monitoring:** Measures body temperature (MLX90614 contactless sensor), heart rate, and SpO₂ (MAX30102 sensor).
* **Automated Emergency Alerts:** Integrated **Telegram Bot API** sends immediate alert notifications to caregivers' phones upon detecting abnormal health metrics.
* **On-Device Display:** Displays live telemetry metrics locally on a 0.96" I²C OLED screen.
* **Web Dashboard & Remote Contact:** Mobile-friendly interface for continuous tracking and a direct "Contact Doctor" form powered by **Formspree** to email health updates to doctors.
* **Portable Power System:** Powered by two 400 mAh Li-ion batteries connected via a TP4056 USB charging and protection module.

## Hardware Components

* **Microcontroller:** ESP32 (Wi-Fi + Bluetooth dual-core)
* **Body Temperature Sensor:** MLX90614 Non-Contact Infrared Thermometer
* **Heart Rate & SpO₂ Sensor:** MAX30102 Pulse Oximetry Module
* **Display:** 0.96" OLED Display (I²C)
* **Power Management:** 2x 400 mAh Li-ion Batteries + TP4056 Micro-USB Charging Module

## System Architecture & Tech Stack

* **Embedded Firmware:** C++ / ESP32 Arduino Core
* **Web Dashboard & Form Handling:** HTML5, CSS3, JavaScript, Formspree API
* **IoT & Communication Protocols:** Telegram Bot API, Wi-Fi / HTTPS client

## Hardware Integration
<p align="center">
  <img height="220" alt="hardware2" src="https://github.com/user-attachments/assets/46581d71-178c-4cee-bcba-5b2a37482135" />
  &nbsp;&nbsp;
  <img height="220" alt="hardware" src="https://github.com/user-attachments/assets/a4c403e7-35cc-4cd3-9836-f7a24c879e9c" />
</p>





## Getting Started

### Hardware Setup
1. Connect the MLX90614 and MAX30102 sensors to the ESP32 via the I²C bus pins (SDA/SCL).
2. Wire the 0.96" OLED display along the same I²C bus.
3. Connect the dual 400 mAh batteries in parallel to the TP4056 module output and route power to the ESP32.

### Firmware & Telegram Configuration
1. Open the project in **VS Code** or **Arduino IDE**.
2. Set your Wi-Fi credentials, **Telegram Bot Token**, and **Chat ID** in your configuration file.
3. Select `ESP32 Dev Module` as your target board and flash the code.

### Web Dashboard & Doctor Contact Setup
1. Open `index.html` or navigate to the `Website/` directory.
2. Update the `form` action attribute inside the Contact Doctor section with your custom **Formspree** endpoint URL.
 <img width="1275" height="681" alt="web dashboard image " src="https://github.com/user-attachments/assets/e24e69a6-d1be-4f86-bc65-08c9759a1d44" />
