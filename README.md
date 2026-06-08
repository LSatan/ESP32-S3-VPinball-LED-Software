# 🚀 ESP32-S3 Ultimate VPin LED Controller (All-in-One Framework)

A high-performance, cost-effective, and fully automated "drop-in replacement" for the classic Teensy controller in Virtual Pinball (VPin) cabinets. 

This framework utilizes the parallel DMA hardware of the ESP32-S3 to drive addressable LEDs on **up to 16 channels absolutely simultaneously**. Designed for extremely fluid lighting effects without stuttering, frame drops, or complex software configurations.

![Hardware Prototype](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/img/hardware/esp32s3.png)  
*(Image: Hardware prototype)*

---

## ✨ Features & Highlights

* **Triple-Connect Technology:** Supports your choice of three different communication methods with the DirectOutput Framework (DOF):
  * **USB (Native CDC):** Classic serial mode (Plug & Play).
  * **WiFi (UDP):** Wireless freedom for standalone toys.
  * **Ethernet (W5500 via UDP):** The absolute premier class for maximum, uncompromising framerates without any USB bottlenecks.
* **8 or 16 Parallel Channels:** No more daisy-chain latency. All channels are written to simultaneously in a single hardware clock cycle.
* **Auto-Config (NVS):** Never hardcode LED lengths again! The controller reads the table layout live from DOF and configures its RAM fully automatically on-the-fly. (only for wifi / ethernet version)
* **FPS RGB Status LED:** A directly addressable status LED on the board gives you color-coded live feedback about your system's current framerate (e.g., White = 120+ FPS, Red = < 20 FPS).
* **Hardware Frequency Output (Clock/Freq Pin):** A dedicated output pin that triggers on every rendered frame. Perfect for physically measuring the exact latency and refresh rate of the ESP32 using an oscilloscope or multimeter.

---

## 📦 The "All-in-One" Package

This repository is a complete "All-in-One" package. You don't have to worry about incompatible external libraries. 
The `src` folder contains **custom-tailored, heavily modified versions** of the `NeoPixelBus` and `Ethernet` libraries. These have been deeply patched for RAM stability, DMA memory release, and TCP overhead reduction in order to process the massive 100+ FPS data streams from DOF.

### ⚙️ Installation
1. Download this project as a ZIP file.
2. Extract the **entire content** into your Arduino Libraries folder (usually `Documents/Arduino/libraries/`).
3. **Strictly required:** Install exactly the **ESP32 Core Version `2.0.17`** via the Arduino Board Manager. (Newer 3.x versions break compatibility with the DMA timers).

*⚠️ Note: Please never update the internal libraries via the Arduino Library Manager, as this will overwrite our hardware patches!*

---

## 📊 Benchmarks & Performance Results

The response time and framerate (FPS) of a pinball cabinet directly depend on how fast data reaches the microcontroller from the PC. Here are our measured, real-world results using the custom DOF network plugin.

*(Note: For network drivers (WiFi/Ethernet), our modified C# DOF DLL is required. See instructions in the example folders).*

### Extreme Tests (Architecture Stress Test)
These tests show the physical limit of the system during massive, symmetrical matrix calculations.

| Setup (16 Channels) | Total LEDs | Native USB | WiFi (UDP) | Ethernet (W5500) |
| :--- | :--- | :--- | :--- | :--- |
| **16 x 200 LEDs** | 3,200 | ~ 22 FPS | ~ 75 - 92 FPS | **~ 85 - 92 FPS** |
| **16 x 300 LEDs** | 4,800 | ~ 15 FPS | ~ 45 - 58 FPS | **~ 60 FPS** |
| **16 x 500 LEDs** | 8,000 | ~ 9 FPS | ~ 29 - 39 FPS | **~ 33 FPS** |

### Real-World Test: Asymmetrical VPin Setup
Very few pinball machines use symmetrical strips. This setup reflects a complex high-end cabinet:
* **2x 144** (Playfield Left & Right)
* **6x 256** (MX Matrix / Addressable Backglass)
* **2x 24** (Speaker Rings)
* **1x 300** (Backboard)
* **1x 300** (Undercab)
* **Total: 2,472 LEDs on 12 channels (asymmetrical)**

| Connection Type | Achieved Framerate | Stability |
| :--- | :--- | :--- |
| **Native USB** | ~ 28 FPS | Heavily fluctuating (USB Bottleneck) |
| **WiFi (UDP)** | ~ 80 FPS | Stable, minimal drops during heavy interference |
| **Ethernet (W5500)** | **~ 129 FPS** | **Rock-solid. No micro-stuttering.** |

---

## 🛠️ Getting Started (Examples)

This framework provides a dedicated, pre-configured sketch for each communication type. 
Please navigate to the `examples` folder, choose your preferred connection type (USB, Ethernet, or WiFi), and follow the instructions there for setting up your `cabinet.xml` and pins.

* [➡️ Go to USB Instructions](examples/USB_Config)
* [➡️ Go to Ethernet (W5500) Instructions](examples/Ethernet_Config)
* [➡️ Go to WiFi Instructions](examples/WiFi_Config)
