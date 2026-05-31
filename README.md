# ESP32-S3 High-Speed USB LED Controller (8/16 Channels) for DirectOutput Framework (DOF)

This project enables the control of large LED installations (e.g., in Virtual Pinball Cabinets) using an ESP32-S3 via USB. It serves as a high-performance alternative to the classic Teensy and utilizes the dedicated hardware of the ESP32-S3 to drive addressable LEDs on up to 16 channels in parallel.

## 🚀 Performance & Architecture

The backbone of this code is the utilization of the ESP32-S3 LCD-DMA method. As a result, all pixels across the 8 or 16 channels are pushed **completely in parallel and at extremely high speed** directly to the pins without straining the CPU.

### The Current Bottleneck (Profiling)
This project was extensively profiled at the hardware level. **Important: The following data is not based on theory, but on real physical measurement results directly from the microcontroller.**

Profiling a single frame with 2,400 LEDs shows the following distribution:
* **~85 % of the time:** Waiting for USB data reception from Windows/DOF.
* **~1 % of the time:** Internal data processing and assignment.
* **~8.5 % of the time:** The actual LED output to the pins (`Show()` / `CanShow()`).

**What this means for your Framerate (FPS):**
Because the Windows driver's serial USB transmission throttles the data stream, the refresh rate (Hz) depends heavily on the total number of LEDs. The transfer scales almost linearly:
* **2,400 LEDs:** limited to **~30 FPS (Hz)**
* **1,100 LEDs:** limited to **~60-65 FPS (Hz)**
* **550 LEDs:** reaches **~110-120 FPS (Hz)**

*(For context: While the human eye generally starts perceiving basic motion as fluid at around 24-30 FPS, fast-paced LED light effects in Virtual Pinball require at least 60 FPS to appear truly smooth and match the refresh rate of modern playfield monitors.)*

The massive processing speed of the ESP32-S3 is currently being throttled by this Windows USB bottleneck. **We are currently working on an alternative solution to completely shatter this serial bottleneck.** Updates will follow shortly.

## 🔌 Hardware Recommendations (Wiring)

For a stable and error-free operation in a pinball cabinet environment, please adhere to these hardware best practices:

* **Level Shifter (3.3V to 5V):** The ESP32-S3 outputs a 3.3V data signal, but WS2812b/addressable LEDs expect a 5V signal. Directly connecting them might work for short distances, but it often leads to flickering or signal drops (especially near solenoids/contactors). It is highly recommended to use a fast Level Shifter (e.g., **74AHCT125** or **SN74HCT245**) on all active data lines.
* **Data Resistor:** Place a resistor (approx. **330Ω to 470Ω**) in series on the data line, ideally between the level shifter and the first LED of each strip. This protects the first pixel from voltage spikes and stabilizes the data signal.

## 🛠 Dependencies

To compile this project without errors, exactly these versions must be installed in the Arduino IDE:

* **ESP32 Core Board Manager:** Version `2.0.17` (Newer 3.x versions can cause compatibility issues with the DMA timers).
* **NeoPixelBus:** By Makuna (Latest version via the Library Manager).

* **Tool settings:** `USB CDC on Boot: "Enable"` `USB Mode: "Hardware CDC and JTAG"`

## ⚙️ DOF Configuration (CabinetConfig.xml)

Integration into the DirectOutput Framework is extremely simple. The ESP32-S3 presents itself to Windows and DOF exactly like a classic Teensy.

### Best Practice for Maximum Speed
It is highly recommended **not** to pass 8 or 16 individual strips from DOF via USB. This creates unnecessary overhead. 
The fastest and most efficient way is to use **only `LedStrip1`** in DOF and enter the **total number of all LEDs** in the cabinet as a continuous block. The distribution of the LEDs to the respective physical pins is handled internally by the code on the ESP32-S3 in fractions of a millisecond.

### Example XML Configuration
Enter the ESP32-S3 in your `CabinetConfig.xml` in the `<OutputControllers>` section as a standard `<TeensyStripController>`. Baud rate settings are not necessary and will be ignored by the S3's native USB.

Default example 2 channels 2x 144 leds. Total 288 leds (also works)
```xml
<OutputControllers>
  <TeensyStripController>
    <Name>LED Strips 0</Name>
    <NumberOfLedsStrip1>144</NumberOfLedsStrip1>
    <NumberOfLedsStrip2>144</NumberOfLedsStrip2>
    <ComPortName>COM8</ComPortName>
    <SendPerLedstripLength>false</SendPerLedstripLength>
    <TestOnConnect>false</TestOnConnect>
  </TeensyStripController>
</OutputControllers>
```

Fire LEDs for all channels through one channel 1x 288 leds. Total 288 leds.
```xml
<OutputControllers>
  <TeensyStripController>
    <Name>LED Strips 0</Name>
    <NumberOfLedsStrip1>288</NumberOfLedsStrip1>
    <ComPortName>COM8</ComPortName>
    <SendPerLedstripLength>false</SendPerLedstripLength>
    <TestOnConnect>false</TestOnConnect>
  </TeensyStripController>
</OutputControllers>
```

In both cases, the rest remains the same.
```xml
  <LedStrip>
    <Name>Left Side</Name>
    <Width>1</Width>
    <Height>144</Height>
    <LedStripArrangement>LeftRightBottomUp</LedStripArrangement>
    <ColorOrder>RGB</ColorOrder>
    <FirstLedNumber>1</FirstLedNumber>
    <FadingCurveName>SwissLizardsLedCurve</FadingCurveName>
    <Brightness>100</Brightness>
    <OutputControllerName>LED Strips 0</OutputControllerName>
  </LedStrip>
  <LedStrip>
    <Name>Right Side</Name>
    <Width>1</Width>
    <Height>144</Height>
    <LedStripArrangement>LeftRightBottomUp</LedStripArrangement>
    <ColorOrder>RGB</ColorOrder>
    <FirstLedNumber>145</FirstLedNumber>
    <FadingCurveName>SwissLizardsLedCurve</FadingCurveName>
    <Brightness>100</Brightness>
    <OutputControllerName>LED Strips 0</OutputControllerName>
  </LedStrip>

............The rest of your XML.
```

## 📝 Important Adjustments in the Arduino Sketch

For the setup to work smoothly and the LEDs to be assigned correctly, the following parameters in the C++ sketch must absolutely be adapted to your cabinet:

1. **Lengths of the physical strips (`stripLengths`):** Since we instruct DOF to send all LEDs via a single strip, the ESP32 needs to know how to split this huge data block back up. You must enter the exact number of LEDs for each of your physical channels into the array in the code. This is the only way the LEDs can be assigned to the correct pins.
2. **Adjust `dofBlockSize`:** If the total number of your LEDs exceeds the default value, the variable `dofBlockSize` (usually defaults to 1100) in the code must be adjusted to the total number of your LEDs.
3. **`BufferSize`:** The serial buffer in the sketch is set to `8192` bytes by default. This buffer is easily sufficient to receive the complete signal of 2,400 LEDs via a single strip in one go (2,400 LEDs * 3 colors = 7,200 bytes of data).
