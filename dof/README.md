### Pull Request Title:
**Add FastUdpController: High-Performance Network Streaming for Addressable LEDs**

### Description:

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


------------------------------------------------------------

**Title:** Resilience Update: Universal High-Availability Watchdog for WemosD1MPStripController (last commit)

### 🛡️ Universal High-Availability & Resilience Architecture (Applies to all Hardware)

Historically, the plugin handled *any* missed serial ACK (even due to a 2ms Windows CPU spike) by throwing a fatal `TimeoutException`, killing the updater thread, and attempting a full controller re-initialization. This caused visible stuttering or permanent LED freezing. This PR introduces a modern, fault-tolerant approach:

* **Streaming Soft-Fail Mitigation:** Missing an ACK (`A`) after a data frame ( `Q`, `O`, `R`) no longer crashes the thread. Instead, the timeout is gracefully caught, the COM port buffers are aggressively purged (`DiscardInBuffer`), and the thread immediately proceeds to the next frame. A dropped frame at high FPS is invisible to the human eye; a full thread crash is not.

* **Optimized Timeouts:** R/W timeouts have been reduced to `50ms` (down from 300ms). This safely accommodates microcontroller DMA reinitialization windows while ensuring that skipped frames due to Windows background tasks do not cause noticeable physical stutters in the cabinet.

* **Hardware Watchdog (EMI & COM-Port Drop Protection):** In a real pinball cabinet, electromagnetic interference (EMI) from contactors or USB hub glitches can cause the virtual COM port to drop for a split second. This triggers a fatal `IOException`. The new Watchdog layer catches this and initiates an intelligent auto-reconnect routine. It will make **5 attempts over a 15-second window** to reclaim the port, resend the `Z` setup configuration, and seamlessly resume the render loop. It is so robust that you can literally unplug the controller during gameplay, plug it back in, and the LEDs will resume instantly.

### 💡 Why this deprecates the "DTR Reset" workaround for ESP32-S2/S3

Previously, users of ESP32-S2 and ESP32-S3 (native USB CDC) were advised to use `DTR=true` to force a hardware/USB stack reset upon reconnection when a frame was dropped.
**This is no longer necessary or recommended.** Tearing down a perfectly healthy physical connection and resetting the microcontroller just because Windows delayed an ACK caused massive lag spikes and blackouts. By implementing the Soft-Fail architecture, the connection remains established, the microcontroller stays in its fast-loop, and transient desynchronizations are handled purely via buffer purging.

 recommended: `<ComPortTimeOutMs>50</ComPortTimeOutMs>` <!-- Reduced from 300ms for Soft-Fail Architecture --> 

----------------------------------------------

**Title:** Add Esp32S3StripController in AdressableLedStrip namespace

**Description:**

This pull request introduces support for the ESP32-S3 architecture (up to 16 channels, bulk data streaming).

### ✨ New Features (S3 / Custom Firmware specific)

* **16-Channel ESP32-S3 Support:** Added properties to unlock channels 11 through 16 for high-end setups, while maintaining 100% backward compatibility with standard 10-channel Wemos D1 setups.

* **Bulk Stream Mode (`EnableBulkMode`):** Replaces the sequential, per-strip ping-pong handshake with a highly efficient, single-payload stream (`W` command). In classic mode, updating e.g. 8 channels adds ~8ms of pure USB overhead (approx. 1ms per channel just for the ACK roundtrip and waiting for the next strip's data), plus the additional delay of the final 'O' (latch) command. Bulk mode completely eliminates this latency by packing the entire cabinet's data into one payload, drastically increasing FPS for large cabinets.

```xml
<OutputControllers>
  <Esp32S3StripController>
    <Name>ESP32_S3_LEDs</Name>
    <ComPortName>COM3</ComPortName> <!-- Change it to your comport name -->
    <ComPortTimeOutMs>50</ComPortTimeOutMs> <!-- We’re changing it from 300 to 50ms to have a smooth transition in case of an error. -->
    <ComPortBaudRate>2000000</ComPortBaudRate>
    <ComPortOpenWaitMs>300</ComPortOpenWaitMs>
    <ComPortHandshakeStartWaitMs>100</ComPortHandshakeStartWaitMs>
    <ComPortHandshakeEndWaitMs>100</ComPortHandshakeEndWaitMs>
    <SendPerLedstripLength>false</SendPerLedstripLength>
    <UseCompression>true</UseCompression>
    <ComPortDtrEnable>false</ComPortDtrEnable> <!-- Leave it on false. No need for dtr reset anymore -->
    <TestOnConnect>false</TestOnConnect>

    <!-- New S3 functions -->
    <Enable16ChannelMode>true</Enable16ChannelMode>
	  <EnableBulkMode>true</EnableBulkMode>

    <NumberOfLedsStrip1>300</NumberOfLedsStrip1>
    <NumberOfLedsStrip2>300</NumberOfLedsStrip2>
    <NumberOfLedsStrip3>150</NumberOfLedsStrip3>
    <NumberOfLedsStrip4>50</NumberOfLedsStrip4>
    <!-- Strips 5 to 16 can be omitted if not used, or set to 0 -->
  </Esp32S3StripController>
</OutputControllers>
```

----------------------------------------------------------------------------

**Title:** Resilience Update: Watchdog & EMI Protection for PinOne NamedPipeServer (Fixes massive Input Lag)

**Description:**

This pull request addresses a critical architectural flaw in the `NamedPipeServer.cs` communication layer that causes severe input lag (often up to 3+ seconds) and stuck contactors when experiencing brief electromagnetic interference (EMI) or transient USB latency.

### 🐛 The Problem
Previously, if the `NamedPipeServer` encountered *any* timeout or write error on the COM port (e.g., due to an EMI glitch from a firing contactor dropping the USB port for a millisecond), it handled the exception by completely destroying the server instance (`isRunning = false`). 
This forced the `PinOneCommunication` client (running on the DOF thread) to sleep for 300ms and attempt to rebuild the entire server. If the port wasn't immediately ready, this loop repeated. Just 10 dropped frames resulted in a cascading **3-second complete freeze** of the DOF pipeline, causing massive flipper button lag and stuck solenoids because the "turn off" commands were caught in the traffic jam.

### 🛡️ The Solution (High-Availability Watchdog)
This update refactors the `NamedPipeServer` to gracefully handle hardware drops without tearing down the IPC (Inter-Process Communication) pipe:

* **Soft-Fail Mitigation:** R/W timeouts have been reduced from an excessive `500ms` down to `50ms`. If a `TimeoutException` occurs, the server silently drops the frame instead of crashing.
* **Continuous Client Acknowledgment:** The most important fix: The server now *always* returns an `"OK"` response to the client, even if a frame was dropped. This entirely prevents the DOF thread from entering its fatal 300ms sleep/reconnect loop.
* **EMI Auto-Recovery:** If a hard exception occurs (e.g., COM port dropped due to EMI), the server safely closes the dead serial handle. On the very next incoming `WRITE` request, it automatically re-opens the port seamlessly under the hood.
* **Comprehensive Logging:** Added distinct console warnings (`[PinOne Watchdog]`) to help users diagnose their physical hardware (distinguishing between minor USB stutters and full physical EMI drops) without interrupting gameplay.

With these changes, the PinOne plugin acts like a tank. You can physically unplug the USB cable mid-game, plug it back in, and it will gracefully resume sending output data immediately once Windows re-enumerates the port, without ever lagging the flipper buttons.
