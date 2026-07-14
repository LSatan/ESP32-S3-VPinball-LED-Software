# 🔌 USB Configuration (Native CDC)

**Recommended for: Small to massive LED setups.**

This sketch turns your ESP32-S3 into a blazing-fast, intelligent drop-in replacement for the Wemos D1 Mini Pro controller. It uses the native USB capabilities of the ESP32-S3 to receive serial data from the DirectOutput Framework (DOF).

---

## ⚠️ PREREQUISITE: Custom DirectOutput.dll

The standard DOF Wemos plugin only supports 10 channels. To unlock the full 16 channels for your ESP32-S3, you **must** replace your existing `DirectOutput.dll` with the modified version provided in this repository.

---

## 🚀 Ultra-Fast LCD DMA Engine (Goodbye FastLED!)

While the original Wemos firmware relies on the standard FastLED library, this ESP32-S3 firmware utilizes the hardware-driven **LCD DMA engine** (via NeoPixelBus). This means LED data is pushed flawlessly in the background without blocking the CPU. Combined with Native USB, this provides unmatched performance.

---

## 🧠 Smart Auto-Detect (No More Hardcoding!)

In older controllers or previous sketches, you had to manually enter your physical LED strip lengths into the Arduino code. **This is no longer necessary!**

The ESP32-S3 now features a smart auto-detect mechanism. It analyzes the very first data frame sent by DOF, learns your exact setup (whether you use 2 or 16 channels, and their exact lengths), and automatically configures its internal hardware DMA buffers on the fly. 
If you add or remove LEDs later, simply update your DOF config—the ESP32 will adapt automatically **the moment you start a table or restart DOF**. No hardware reboot required!

---

## ⚡ Bulk Stream Mode & Compression (NEW)

This sketch now supports two powerful features to maximize your framerates:

1.  **Bulk Stream Mode (`EnableBulkMode`):** 
    Instead of the classic ping-pong handshake (where DOF sends data for Strip 1, waits for an ACK, then sends Strip 2, waits, etc.), **Bulk Mode** packs the data for *all* your LED strips into a single, massive payload. This completely eliminates USB overhead latency and drastically increases the FPS for large cabinets with many channels.
2.  **Color-based RLE Compression (`UseCompression`):**
    Reduces the USB payload size heavily. Combined with Bulk Mode and the LCD DMA engine, this ensures butter-smooth 60-120+ FPS even on gigantic setups (depending on effects and longest strip).

---

## 💻 Arduino IDE Settings (ESP32-S3)

To utilize the Native USB speed and avoid serial timeouts, you must configure the Arduino IDE `Tools` menu **exactly** like this before flashing:

**Key Settings Checklist:**
* **Board:** ESP32S3 Dev Module
* **USB CDC On Boot:** `Enabled` *(CRITICAL: Routes serial data through the fast native USB port)*
* **Arduino Runs On:** `Core 0` *(CRITICAL: Fixes cross-core USB timeouts during high-speed compression)*
* **USB Mode:** `Hardware CDC and JTAG`
* **Upload Mode:** `UART0 / Hardware CDC`
* **CPU Frequency:** `240MHz`
* **Flash Mode:** `QIO 80MHz`

---

## ⚙️ Step 3: DOF Configuration (`cabinet.xml`)

Add the `Esp32S3StripController` to your DirectOutput configuration. 
* `Enable16ChannelMode`: Set to `true` to unlock pins 11-16.
* `EnableBulkMode`: Set to `true` to enable single-payload streaming (requires the latest custom DOF .dll).
* `UseCompression`: Set to `true` for maximum performance.
* `SendPerLedstripLength`: Can safely be `false` because of the S3's smart auto-detect.

```xml
<OutputControllers>
  <Esp32S3StripController>
    <Name>ESP32_S3_Controller</Name>
    <ComPortName>COM3</ComPortName> <!-- Change to your COM Port -->
    <ComPortTimeOutMs>50</ComPortTimeOutMs> <!-- Reduced from 300ms for Soft-Fail Architecture -->
    <ComPortBaudRate>2000000</ComPortBaudRate>
    <ComPortOpenWaitMs>300</ComPortOpenWaitMs>
    <ComPortHandshakeStartWaitMs>100</ComPortHandshakeStartWaitMs>
    <ComPortHandshakeEndWaitMs>100</ComPortHandshakeEndWaitMs>
    <SendPerLedstripLength>false</SendPerLedstripLength> <!-- We leave it on false, the controller automatically detects the strip length. -->
    
    <!-- NEW ESP32-S3 FEATURES -->
    <UseCompression>true</UseCompression>
    <EnableBulkMode>true</EnableBulkMode>
    <Enable16ChannelMode>true</Enable16ChannelMode>
    
    <ComPortDtrEnable>false</ComPortDtrEnable> <!-- We'll leave it on false, no longer needed due to Watchdog. -->
    <TestOnConnect>false</TestOnConnect>

    <!-- DEFINE YOUR STRIPS HERE (0 or not specified = Disabled) -->
    <NumberOfLedsStrip1>144</NumberOfLedsStrip1>
    <NumberOfLedsStrip2>144</NumberOfLedsStrip2>
    <NumberOfLedsStrip3>50</NumberOfLedsStrip3>
    <NumberOfLedsStrip4>0</NumberOfLedsStrip4>
    <NumberOfLedsStrip5>0</NumberOfLedsStrip5>
    <NumberOfLedsStrip6>0</NumberOfLedsStrip6>
    <NumberOfLedsStrip7>0</NumberOfLedsStrip7>
    <NumberOfLedsStrip8>0</NumberOfLedsStrip8>
    <NumberOfLedsStrip9>0</NumberOfLedsStrip9>
    <NumberOfLedsStrip10>0</NumberOfLedsStrip10>
    <NumberOfLedsStrip11>0</NumberOfLedsStrip11>
    <NumberOfLedsStrip12>0</NumberOfLedsStrip12>
    <NumberOfLedsStrip13>0</NumberOfLedsStrip13>
    <NumberOfLedsStrip14>0</NumberOfLedsStrip14>
    <NumberOfLedsStrip15>0</NumberOfLedsStrip15>
    <NumberOfLedsStrip16>0</NumberOfLedsStrip16>
  </Esp32S3StripController>
</OutputControllers>
```
