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
If you add or remove LEDs later, simply update your DOF config—the ESP32 will adapt automatically on the next boot.

---

## ⚡ Compression Mode Supported

This sketch now fully supports DOF's built-in Color-based RLE Compression (`Q` command). By enabling this feature in the plugin, the USB payload size is drastically reduced. Combined with the LCD DMA engine, this ensures butter-smooth 60-120+ FPS even on gigantic setups. (depending on effects and longest strip)

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

Add the `WemosD1MPStripController` to your DirectOutput configuration. 
* `Enable16ChannelMode`: Set to `true` to unlock pins 11-16.
* `UseCompression`: Set to `true` for maximum performance.
* `SendPerLedstripLength`: Can safely be `false` because of the S3's smart auto-detect.

```xml
<OutputControllers>
  <WemosD1MPStripController>
    <Name>ESP32_S3_Controller</Name>
    <ComPortName>COM3</ComPortName> <!-- Change to your COM Port -->
    <ComPortTimeOutMs>300</ComPortTimeOutMs>
    <ComPortBaudRate>2000000</ComPortBaudRate>
    <ComPortOpenWaitMs>300</ComPortOpenWaitMs>
    <ComPortHandshakeStartWaitMs>100</ComPortHandshakeStartWaitMs>
    <ComPortHandshakeEndWaitMs>100</ComPortHandshakeEndWaitMs>
    <SendPerLedstripLength>false</SendPerLedstripLength> <!-- We leave it on false, the controller automatically detects the strip length. -->
    <UseCompression>true</UseCompression>
    <ComPortDtrEnable>false</ComPortDtrEnable> <!-- We'll leave it on false, we don't need a DTR reset. -->
    <TestOnConnect>false</TestOnConnect>
    <Enable16ChannelMode>true</Enable16ChannelMode> <!-- NEW ESP32-S3 FEATURES -->

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
  </WemosD1MPStripController>
</OutputControllers>
```
