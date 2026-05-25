# Attention: There are currently still errors that are being fixed!

# ESP32-S3 VPinball LED Software (High Performance)

An ultra-high-performance, cost-effective, and native drop-in replacement for the legendary Teensy 3.2 and Teensy 4.1 controllers in Virtual Pinball (vPin) cabinets. Supporting both **8-channel** and **16-channel** parallel DMA outputs.

---

## Introduction: ESP32-S3 – The DOF Teensy Killer

For years, the **Teensy 3.2** (and later the **Teensy 4.1**) paired with an OctoWS2811 shield has been the gold standard for controlling addressable LED strips (side-board matrix, backglass matrix, undercabinet lighting) in Virtual Pinball setups via the DirectOutput Framework (DOF). 

However, Teensy setups have become highly impractical for the community:
* **Teensy 3.2** is completely discontinued, making it impossible to buy or forcing users to pay extreme scalper prices.
* **Teensy 4.1** is widely available but highly expensive, oversized, and severely overpowered for simply shifting LED bytes.
* The official **OctoWS2811 adaptor boards** add extra cost, delivery times, and physical bulk to the build.

This project introduces a **true native alternative**: The **ESP32-S3**. By utilizing its advanced internal hardware peripherals, the ESP32-S3 achieves the exact same real-time performance and refresh rates as a Teensy at a fraction of the cost. It acts as a native drop-in replacement, meaning **no changes are required on the PC side or within DOF**.

---

## Hardware & Cost Comparison: Teensy Setup vs. ESP32-S3 Setup

Below is a realistic, real-world cost and hardware comparison of getting a fully stable, flicker-free, multi-channel addressable LED setup running in a cabinet.

| Feature | Teensy 3.2 Setup (Legacy) | Teensy 4.1 Setup (Current) | ESP32-S3 Setup (This Project) |
| :--- | :--- | :--- | :--- |
| **Processor** | ARM Cortex-M4 @ 120 MHz | ARM Cortex-M7 @ 600 MHz | **Xtensa Dual-Core LX7 @ 240 MHz** |
| **Controller Cost** | Discontinued (Scalper: $50+) | $30.00 – $38.00 USD | **$4.00 – $7.00 USD** (ESP32-S3 DevKit) |
| **Required Interface** | $10.00 – $15.00 USD (OctoWS2811) | $10.00 – $15.00 USD (OctoWS2811) | **$0.50 – $1.50 USD** (74HCT245 or SN74AHCT125N) |
| **Total Hardware Cost**| **~$60.00+ USD** | **$40.00 – $53.00 USD** | **$4.50 – $8.50 USD** |
| **Parallel Channels** | 8 channels | 8 channels | **8 or 16 channels** (Parallel Hardware DMA) |
| **USB Interface** | Native USB Full-Speed | Native USB High-Speed | Native USB OTG (CDC Full-Speed) |
| **Availability** | Out of stock / Dead | Available but expensive | **Instantly available worldwide** |

### Why Teensy is Overpowered & Where the Limits Lie
The Teensy's CPU clock speeds are impressive, but running them solely to pull bytes out of a USB buffer and push them down an LED strip is an extreme underutilization of resources. 

The bottleneck in addressable LED setups is **never the raw CPU clock speed**. It is defined by two fixed limits:
1. **The WS2812B Protocol Limit:** Every single WS2812B LED requires exactly **30 microseconds (µs)** to receive its 24-bit color data. Pushing data sequentially means the more LEDs you have, the lower your frame rate drops.
2. **USB Buffer Saturations:** High-refresh-rate cabinets (4K @ 120Hz/240Hz) flood the controller with data frames every few milliseconds. If the controller's serial buffer is too small or processing blocks the incoming stream, bytes are lost, resulting in DOF timeout exceptions.

### Why the ESP32-S3 is Just as Fast
The ESP32-S3 features a specialized internal hardware peripheral: the **LCD Master Parallel Interface**. Combined with **Direct Memory Access (DMA)**, the ESP32-S3 can output data to **8 or 16 pins simultaneously (in parallel)** entirely in the background. 

The CPU simply prepares the memory buffer and hands it over to the hardware engine. Pushing data to 16 channels takes the exact same amount of time as pushing it to a single channel.

---

## Performance Metrics & Technical Specifications

### The 120Hz Frame Budget
To synchronize perfectly with modern 4K 120Hz gaming monitors, the entire cabinet lighting cycle must complete within a frame budget of **8.33 milliseconds**.
* Because the ESP32-S3 transmits data to all channels in parallel, the total transmission time is determined **only by the single longest strip connected to the board**.
* **Maximum Strip Length for 120Hz:** `8.33 ms / 0.03 ms per LED = ~277 LEDs` per channel. 
* As long as no single channel exceeds ~270 LEDs, your cabinet will run at a flawless, native 120Hz refresh rate.

### Native USB Speed
The ESP32-S3 connects directly to the PC via its native USB On-The-Go (OTG) peripheral configured as a Virtual COM Port (USB CDC). It completely bypasses traditional hardware UART baud rate restrictions. Data is transferred at native USB Full-Speed (up to 12 Mbps), filling the controller's buffer instantly.

---

## Native Compatibility & Smart Routing

This firmware implements the original Teensy assembly protocol natively. It responds to all standard DOF serial commands:
* `L` (Set Strip Length)
* `R` (Receive LED Data)
* `O` (Output / Show LEDs)
* `C` (Clear All)
* `F` (Fill Block)
* `V` (Firmware Version Handshake)
* `M` (Protocol Marker)

### The "Smart-Decoder" Architecture
Older firmware variants required hardcoded block sizes (like the rigid 1100-step Teensy divider) which forced DOF to pad the USB stream with thousands of empty, black "dummy" pixels just to align the channels. 

This firmware is smart:
1. Upon startup, it intercepts the `L` command from DOF to dynamically read the exact block length configured in your `cabinet.xml`.
2. It parses incoming sequential stream indexes automatically using this dynamic block size (`channel = dofIndex / dofBlockSize`).
3. It filters out unneeded padding bytes using a local hardware layout array (`stripLengths`), preventing memory overflow and eliminating signal degradation on shorter strips.

---

## Hardware Requirements & Wiring

### 1. Level Shifter (Mandatory!)
The ESP32-S3 operates on **3.3V logic**, whereas WS2812B LED strips require a **5V logic signal**. 
* **DO NOT use generic bi-directional I2C level shifters** (the small red/blue boards with MOSFETs and pull-up resistors like the **TXS0108E**). They have slow rise times and will completely distort the high-speed 800kHz WS2812B data signal, causing severe flickering and direction-sensing loops when driving long cabinet wires.
* **Recommended:** Use a fast CMOS level shifter like the **SN74AHCT125N** (4 channels, DIP-14, highly recommended for easy soldering) or **74HCT245** (8 channels). These ICs easily handle the nanosecond transition speeds and active push-pull drive strength required for clean, robust LED signals.

### 2. Data Line Resistors
Place a **330 Ω to 470 Ω resistor** on each data line between the level shifter output and the first LED of the strip. This prevents voltage spikes and impedance reflections from damaging the first pixel.

### 3. Common Ground
Ensure that the Ground (GND) of the ESP32-S3, the Ground of the Level Shifter, and the Ground of the LED Power Supplies are **physically tied together**. Missing common grounds are the #1 cause of data corruption.

---

## Software Dependencies & Installation

### Required Libraries & Cores
1. **Arduino ESP32 Board Core:** **Version 2.0.17** is strictly required! 
   * *Note:* **Do not use v3.x cores**, as breaking changes in the ESP32 network/LCD APIs will cause compilation errors with the parallel DMA methods of NeoPixelBus.
2. **NeoPixelBus by Makuna:** Install the latest version via the Arduino Library Manager. (Utilizes the ultra-efficient `NeoEsp32LcdX8Ws2812xMethod` and `NeoEsp32LcdX16Ws2812xMethod` hardware engines).

### Arduino IDE Tools Settings
When flashing the ESP32-S3, you **must** apply the following settings under the **Tools** menu, otherwise native USB communication will fail:

* **Board:** `ESP32S3 Dev Module` (or your specific S3 board variant)
* **USB CDC On Boot:** `Enabled`  <-- **CRITICAL!** Maps the native USB port directly to `Serial`.
* **Flash Size:** `4MB (32Mb)` (or matching your board)
* **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)`
* **Upload Speed:** `921600`

---

## Sketch Customization

Before uploading either the 8-channel or 16-channel sketch, adapt the hardware configurations at the top of the file to match your physical cabinet layout:

```cpp
// Define how many physical LEDs are connected per channel
const uint16_t stripLengths[ChannelCount] = { 
    144, 144, 24, 24, 144, 144, 144, 144 
};

// Define the GPIO pins mapped to each channel
const uint8_t pins[ChannelCount] = { 
    4, 5, 6, 7, 8, 9, 10, 11 
};

// Power-On Self-Test feature
#define ENABLE_LED_TEST 1 // Set to 1 to cycle Red, Green, Blue on startup
```

### PC Configuration (`cabinet.xml`)
In your DirectOutput `cabinet.xml`, configure the controller exactly like a native Teensy strip controller. 
* Set your port assignments normally (e.g., RGB strips occupy 3 sequential ports like `P:001`, `P:002`, `P:003`).
* Set `<SendPerLedStripLength>` to `false` for standard, rock-solid block transmission.

---

## Example Real-World Configuration

Here is a complete, production-tested example of a `cabinet.xml` setup using this firmware for a dual sideboard layout (Left Side and Right Side, 144 addressable LEDs each) running on `COM8`.

> **Note on Baud Rate Configuration:** The baud rate inside the configuration is intentionally declared at `12000000` (12 Mbps). While the native hardware USB OTG layer of the ESP32-S3 handles data streams at raw internal bus speeds and ignores old serial speed boundaries, defining a steady 12 Mbps cap on the host application side ensures that the Windows USB-CDC driver allocates optimal full-speed polling windows without artificial software throttling bottlenecks.

### `cabinet.xml`
```xml
<?xml version="1.0"?>
<Cabinet xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">

  <Name>My Pin Cab</Name>

  <OutputControllers>
    <TeensyStripController>
      <Name>LED Strips 0</Name>
      <NumberOfLedsStrip1>144</NumberOfLedsStrip1>
      <NumberOfLedsStrip2>144</NumberOfLedsStrip2>
      <ComPortName>COM8</ComPortName>
      <ComPortBaudRate>12000000</ComPortBaudRate>
      <SendPerLedstripLength>true</SendPerLedstripLength>
      <TestOnConnect>false</TestOnConnect>
    </TeensyStripController>
  </OutputControllers>

  <Toys>  
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
      
    <LedWizEquivalent>
      <Name>LedWizEquivalent 30</Name>
      <LedWizNumber>30</LedWizNumber>
      <Outputs>
        <LedWizEquivalentOutput>
          <OutputName>Left Side</OutputName>
          <LedWizEquivalentOutputNumber>1</LedWizEquivalentOutputNumber>
        </LedWizEquivalentOutput>
        <LedWizEquivalentOutput>
          <OutputName>Right Side</OutputName>
          <LedWizEquivalentOutputNumber>4</LedWizEquivalentOutputNumber>
        </LedWizEquivalentOutput>
      </Outputs>
    </LedWizEquivalent>
  </Toys>

</Cabinet>
```

### Corresponding DirectOutput (DOF) Config Tool Settings

#### 1. Controller Assignments (LedWiz Selection)
![DOF Device Setup](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/img/dof_config_example_1.png)

#### 2. Combine Toys
![DOF Config Combine Toys](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/img/dof_config_example_2.png)

#### 3. Port Mapping
![DOF Config Port Mapping](https://github.com/LSatan/ESP32-S3-VPinball-LED-Software/blob/main/img/dof_config_example_3.png)

---

## License & Credits
This project is released under the MIT License. Built upon the excellent `NeoPixelBus` library by Makuna and inspired by the Virtual Pinball community.
