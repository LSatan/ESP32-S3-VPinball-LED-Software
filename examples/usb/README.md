# 🔌 USB Configuration (Native CDC)

**Recommended for: Small to medium LED setups.**

This sketch turns your ESP32-S3 into a blazing-fast, drop-in replacement for the classic Teensy controller. It uses the native USB capabilities of the ESP32-S3 to receive serial data from the DirectOutput Framework (DOF).

---

## 🧠 The "Single Channel" Trick (Crucial for High FPS)

You might wonder why we don't configure 16 separate strips inside the DOF `cabinet.xml` for this USB setup. 

**The reason is a bottleneck in the original Teensy protocol:** The standard Teensy protocol used by DOF relies on a "ping-pong" handshake communication. Furthermore, DOF struggles natively if you assign more than 10 individual strips to a Teensy. If we let DOF send 16 separate strip commands, the handshake overhead adds about **~10ms of delay per frame**, which massively degrades the framerate.

**Our Solution:** We trick DOF! In your `cabinet.xml`, you configure **only ONE giant LED strip** containing the total sum of all your LEDs (e.g., 2,472 LEDs). DOF packages all colors into one massive, contiguous data block and fires it over USB in a single sweep without ping-pong delays.
The ESP32-S3 receives this giant block and acts as an intelligent splitter, slicing the data internally and routing it to the parallel hardware pins.

---

## 🛠️ Step 1: Manual Length Configuration (In the Sketch)

Because DOF only sends one giant data block via USB without layout headers, the ESP32 **must know** how many LEDs are connected to each physical pin so it can slice the data correctly.

Open the `.ino` sketch and adjust the `stripLengths` array at the top of the code to match your physical wiring. 

*Example for an asymmetrical VPin setup:*
```cpp
// --- YOUR PHYSICAL HARDWARE ---
// Enter the exact number of LEDs for each of your pins!
// Example: Pin 1 (144), Pin 2 (144), Pin 3 (300), Pin 4 (48), Pins 5-16 (0)
const uint16_t stripLengths[ChannelCount] = { 144, 144, 300, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
```
*Ensure the order exactly matches the assigned pins in the `pins[]` array!*

---

## 💻 Step 2: Arduino IDE Settings (ESP32-S3)

To utilize the Native USB speed and avoid serial bottlenecks, you must configure the Arduino IDE `Tools` menu exactly like this before flashing:

**Key Settings Checklist:**
* **Board:** ESP32S3 Dev Module
* **USB CDC On Boot:** `Enabled` *(CRITICAL: This routes the serial data directly through the fast native USB port!)*
* **USB Mode:** `Hardware CDC and JTAG`
* **Upload Mode:** `UART0 / Hardware CDC`
* **CPU Frequency:** `240MHz`
* **Flash Mode:** `QIO 80MHz`

*Note: Since this relies on Native CDC, the "Baud Rate" setting in Windows Device Manager or DOF is completely irrelevant. The data will transfer at maximum USB bus speed automatically.*

---

## ⚙️ Step 3: DOF Configuration (`cabinet.xml`)

Add a standard TeensyStripController to your DirectOutput configuration. Remember: configure the `NumberOfLeds` as the **total sum** of all your strips combined!

```xml
<OutputControllers>
  <TeensyStripController>
    <Name>ESP32_Teensy_Replacement</Name>
    <NumberOfLeds>2472</NumberOfLeds>
    <ComPortName>COM3</ComPortName>
    <TestOnConnect>false</TestOnConnect>
  </TeensyStripController>
</OutputControllers>
```
