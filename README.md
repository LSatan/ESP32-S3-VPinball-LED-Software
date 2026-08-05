# 🚀 ESP32-S3 Ultimate VPin LED Controller (All-in-One Framework)

A high-performance, cost-effective, and fully automated "drop-in replacement" for the classic Teensy controller in Virtual Pinball (VPin) cabinets. 

This framework utilizes the parallel DMA hardware of the ESP32-S3 to drive addressable LEDs on **up to 16 channels absolutely simultaneously**. Designed for extremely fluid lighting effects without stuttering, frame drops, or complex software configurations.

> **🛠️ Hardware Update:** Gerber, BOM, and Pick & Place files for the custom PCBs will be added here soon, as soon as I receive and test the prototype boards!

![Hardware Prototype](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/hardware/img/esp32s3_board_preview.png)
*(Image: Coming soon)*

---

![Hardware Prototype](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/hardware/img/esp32s3_prototype.png)
*(Image: First Hardware prototype)*

---

## ✨ Features & Highlights

* **All-in-One Config Tool:** Download our complete companion tool to effortlessly flash firmwares, automatically adjust your Windows network adapters and firewall settings, generate your cabinet configuration, and easily install the included custom DOF files directly through the tool.
  👉 **[Download VPin Config Tool (Latest Release)](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/releases/latest)**
* **Dynamic PSU Safety Limit:** The firmware now features a built-in safety mechanism that automatically adapts the LED power draw limit dynamically on every frame to ensure your Power Supply Unit (PSU) is never overloaded.
* **Triple-Connect Technology:** Supports your choice of three different communication methods with the DirectOutput Framework (DOF):
  * **USB (Native CDC / Bulk / RLE):** Multiple USB modes for maximum compatibility and performance.
  * **WiFi (UDP):** Wireless freedom for standalone toys.
  * **Ethernet (W5500 via UDP):** The absolute premier class for maximum, uncompromising framerates without any USB bottlenecks.
* **8 or 16 Parallel Channels:** No more daisy-chain latency. All channels are written to simultaneously in a single hardware clock cycle.
* **Auto-Config (NVS):** Never hardcode LED lengths again! The controller reads the table layout live from DOF and configures its RAM fully automatically on-the-fly. (only for wifi / ethernet version)
* **FPS RGB Status LED:** A directly addressable status LED on the board gives you color-coded live feedback about your system's current framerate (e.g., White = 120+ FPS, Red = < 20 FPS).
* **Hardware Frequency Output (Clock/Freq Pin):** A dedicated output pin that triggers on every rendered frame. Perfect for physically measuring the exact latency and refresh rate of the ESP32 using an oscilloscope or multimeter.

---

## 📦 The "All-in-One" Package

This repository is a complete "All-in-One" package. You don't have to worry about incompatible external libraries. 
The `src` folder contains **custom-tailored, heavily modified versions** of the `NeoPixelBus` and `Ethernet` libraries. These have been deeply patched for RAM stability, DMA memory release, and TCP overhead reduction in order to process the massive data streams from DOF.

### ⚙️ Installation
1. Download this project as a ZIP file.
2. Extract the **entire content** into your Arduino Libraries folder (usually `Documents/Arduino/libraries/`).
3. **Strictly required:** Install exactly the **ESP32 Core Version `2.0.17`** via the Arduino Board Manager. (Newer 3.x versions break compatibility with the DMA timers).

*⚠️ Note: Please never update the internal libraries via the Arduino Library Manager, as this will overwrite our hardware patches!*

---

## 📊 Benchmarks & Performance Results
Test setup: Table VPX BIG Bang / Ball Shoot (Ready Sequence)
This tablet constantly sends data even when all the lights are off.
Effekt: PF Right = (PF Right Flashers MX + PF Right Effects MX) & PF Left = (PF Left Flashers MX + PF Left Effects)
Note: The data is alternately assigned to the LED strips, channels 1 to 16. Quite a challenge for RLE compression (short black sections are interrupted by single purple pixels).

The response time and framerate (FPS) of a pinball cabinet directly depend on how fast data reaches the microcontroller from the PC. Here are our measured, real-world results using the custom DOF network plugin.

*(Note: Our modified C# DOF DLL is required. See instructions in the example folders).*

### Extreme Tests (Architecture Stress Test)
These tests show the physical limit of the system during massive, symmetrical matrix calculations.

| Setup (16 Channels) | Total LEDs | Native USB | USB RLE | USB Bulk | USB Bulk + RLE | WiFi (UDP) | Ethernet (W5500) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **16 x 200 LEDs** | 3,200 | ~ 12 FPS | ~ 27 - 30 FPS | ~ 12 FPS | ~ 48 - 50 FPS | ~ 75 - 92 FPS | **~ 85 - 92 FPS** |
| **16 x 300 LEDs** | 4,800 | ~ 7 FPS | ~ 25 FPS | ~ 8 FPS | ~ 34 FPS | ~ 45 - 58 FPS | **~ 60 FPS** |
| **16 x 500 LEDs** | 8,000 | ~ 4 FPS | ~ 16 - 18 FPS | ~ -- FPS | ~ 20 FPS | ~ 29 - 39 FPS | **~ 38 FPS** |

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
| **Native USB** | ~ 14 FPS | USB Bottleneck |
| **USB RLE** | ~ 26 FPS | playable |
| **USB Bulk** | ~ 15 FPS | USB Bottleneck |
| **USB Bulk + RLE** | ~ 33 FPS | playable |
| **WiFi (UDP)** | ~ 80 FPS | Stable, minimal drops during heavy interference |
| **Ethernet (W5500)** | **~ 80 FPS** | **Rock-solid. No micro-stuttering.** |

Result: Frequencies above 24 FPS + are perceived as smooth.
RLE compression performs much better with a real DOF config since, for example, the undercab usually has a consistent color.
The UDP protocol doesn’t care about the colors of the current frame. They stay stable whether it’s black, colorful, etc.

---

## 🛠️ Getting Started (Examples)

This framework provides a dedicated, pre-configured sketch for each communication type. 
Please navigate to the `examples` folder, choose your preferred connection type (USB, Ethernet, or WiFi), and follow the instructions there for setting up your `cabinet.xml` and pins.

* [➡️ Go to USB Instructions](examples/usb)
* [➡️ Go to Ethernet (W5500) Instructions](examples/ethernet)
* [➡️ Go to WiFi Instructions](examples/wifi)
