### Pull Request Title:
**Add FastUdpController: High-Performance Network Streaming for Addressable LEDs**

### Description:
This PR introduces the `FastUdpController` inside a newly created `DirectOutput.Cab.Out.Network` namespace. 

It is designed to stream addressable LED data via UDP to microcontrollers (specifically optimized for ESP32-S3 with W5500 LAN modules or WiFi setups). By eliminating USB bottlenecks and implementing a custom packet-pacing algorithm, this controller easily handles large, asymmetrical LED layouts (e.g., 16 strips with varying lengths up to thousands of LEDs) while maintaining a rock-solid framerate of 100+ FPS without triggering buffer overflows on the receiver end.

#### 🔄 Protocol Flow & Chunking Logic
Since a full frame of 16 heavily populated LED strips quickly exceeds the standard network MTU (1500 bytes), the controller processes the output as follows:
1. **Buffer Assembly:** It grabs the current LED colors for all active strips and assembles them into a single, large byte array.
2. **Layout Injection:** It prepends a 33-byte layout header to this array so the microcontroller always knows the exact strip lengths for the current frame.
3. **Chunking:** The large buffer is split into smaller UDP payload chunks (max 1400 bytes).
4. **Burst Pacing (Hardware Protection):** To prevent overloading the hardware SPI buffers of modules like the W5500, a dynamic burst brake is applied. After sending a burst of chunks (e.g., 14 KB), a 4ms delay allows the microcontroller to clear its RX buffers before the rest of the frame arrives.

#### 📦 Packet Structure
To ensure the microcontroller can reconstruct the frame seamlessly, **3 identification bytes** are attached to the very front of *every single UDP chunk*:

| Byte Index | Name | Description |
| :--- | :--- | :--- |
| `Byte 0` | **Frame ID** | Rolls over from 0 to 255. Tells the ESP32 if the chunk belongs to the current frame. If the ID changes, the ESP resets its receiver buffer. |
| `Byte 1` | **Chunk Index** | The current position of this packet (e.g., `0`, `1`, `2`...). |
| `Byte 2` | **Total Chunks** | The total amount of packets needed to complete this specific Frame ID. |

**The Payload (Starting at Byte 3 of the first chunk):**
The very first chunk of a frame always starts with the 33-byte Layout Header:
* `1 Byte`: Number of max strips (Fixed to 16).
* `32 Bytes`: The lengths of strips 1 to 16 (represented as 16x High Byte / Low Byte pairs).
* `Remaining Bytes`: The sequential RGB data for all configured LEDs.

#### ⚙️ Example `cabinet.xml` Configuration
Here is an example of how a user configures this controller in their cabinet setup. They just need to provide the IP of their ESP32, the UDP Port (default 6454), and the physical amount of LEDs per strip.

```xml
<OutputControllers>
  <FastUdpController>
    <Name>ESP32_Network_LEDs</Name>
    <IpAddress>192.168.1.100</IpAddress>
    <Port>6454</Port>
    <NumberOfLedsStrip1>300</NumberOfLedsStrip1>
    <NumberOfLedsStrip2>300</NumberOfLedsStrip2>
    <NumberOfLedsStrip3>150</NumberOfLedsStrip3>
    <NumberOfLedsStrip4>50</NumberOfLedsStrip4>
    <!-- Strips 5 to 16 can be omitted if not used, or set to 0 -->
  </FastUdpController>
</OutputControllers>
```

This addition is completely isolated, does not interfere with existing serial/Teensy implementations, and provides a much-needed robust network alternative for modern VPin builds.
