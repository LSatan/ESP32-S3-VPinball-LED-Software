# 📡 WiFi Configuration (Access Point / UDP)

**Recommended for: Small to massive LED setups, zero extra hardware costs, wireless toys.**

This setup utilizes the built-in WiFi antenna of the ESP32-S3 to receive high-speed UDP data streams directly from your Virtual Pinball PC. It completely eliminates the USB bottleneck without requiring the purchase of an external Ethernet module (like the W5500).

---

## 🚀 The "Auto-Config" Magic (Zero Code Editing)

Unlike the USB serial protocol, our custom C# DirectOutput network plugin transmits a "Layout Header" at the start of every UDP frame. 

**What this means for you:** You never have to configure or hardcode any LED strip lengths inside the Arduino sketch! The ESP32-S3 dynamically reads the strip lengths directly from the incoming DOF signal and automatically allocates the correct DMA memory in the background. If you change your layout in DOF, the ESP32 adapts instantly on the fly. 

*Simply flash the sketch, and you're done with the code!*

---

## ⚠️ Important: Standalone AP Mode ONLY

This firmware is strictly designed to run the ESP32-S3 in **Access Point (AP) Mode**. 
The ESP32-S3 will broadcast its own wireless network (e.g., `VPin_LED_Controller`). Your VPin PC must connect *directly* to this network via a WiFi adapter/dongle.

**Why not connect the ESP32 to my home router?**
Routing a 100+ FPS uncompressed UDP data stream through a standard household router (which is busy managing smartphones, smart TVs, and internet traffic) introduces unpredictable ping spikes, packet drops, and jitter. This completely ruins the fluid LED animations. A direct PC-to-ESP connection guarantees a highly stable, interference-free "Fire & Forget" UDP pipeline.

*(Note: You can still use your PC's standard Ethernet port for internet access and normal network duties while the WiFi adapter handles the dedicated ESP32 connection).*

---

## 💻 Step 1: Arduino IDE Settings (ESP32-S3)

Before flashing the WiFi sketch, make sure your Arduino IDE `Tools` menu is configured correctly. 

**Crucial Difference to USB:** You must set `USB CDC On Boot` to **Disabled**! Leaving it enabled while using WiFi can cause the ESP32 to freeze if no serial monitor is actively listening via USB.

**Key Settings Checklist:**
* **Board:** ESP32S3 Dev Module
* **USB CDC On Boot:** `Disabled` *(CRITICAL!)*
* **USB Mode:** `Hardware CDC and JTAG`
* **Upload Mode:** `UART0 / Hardware CDC`
* **CPU Frequency:** `240MHz (WiFi)` *(WiFi requires maximum CPU power)*
* **Flash Mode:** `QIO 80MHz`

---

## 🛡️ Step 2: Windows Network & Firewall Settings

When you connect your PC to the newly created ESP32-S3 WiFi network, Windows usually classifies it as a "Public Network". By default, the Windows Firewall aggressively blocks outgoing UDP traffic to unrecognized public networks.

**To ensure DOF can send data to the ESP32:**
1. Connect your PC to the ESP32's WiFi (Default SSID: `VPin_LED_Controller`, Password: `vpinpassword`).
2. Open **Windows Defender Firewall with Advanced Security**.
3. Create a new **Outbound Rule**.
4. Select **Port** -> **UDP**.
5. Specify the local and remote port: **`6454`** *(This is the default Art-Net/DOF port)*.
6. Allow the connection for all network profiles (Domain, Private, Public).

*Note: If you already use port 6454 for other hardware, you can easily change the port in both the C# Plugin source code and the ESP32 sketch.*

---

## ⚙️ Step 3: DOF Configuration & Custom DLLs

⚠️ **IMPORTANT: Custom DOF DLL Replacement Required!**
Because our `FastUdpController` is currently pending a Pull Request and is not yet included in the official DirectOutput release, you **must** replace your existing DOF DLLs with the modified ones provided in this package.

1. Locate the **DOF_x86** and **DOF_x64** folders provided in this project download.
2. Navigate to your existing DirectOutput installation directory on your PC.
3. Replace the original DLL files in your local `x86` and `x64` folders with our provided versions.
*(Don't worry: Our DLLs are compiled directly from the latest official mjr source code branch. You won't lose any existing features!)*

After replacing the DLLs, add the custom controller to your `cabinet.xml`. You only need to define the strips you actually use; you can delete or set unused strips to `0`.

```xml
<OutputControllers>
  <FastUdpController>
    <Name>ESP32_WiFi_Controller</Name>
    <IpAddress>192.168.4.1</IpAddress> <!-- Default IP of the ESP32 AP -->
    <Port>6454</Port>
    
    <!-- Define your physical setup here (up to 16 strips) -->
    <NumberOfLedsStrip1>144</NumberOfLedsStrip1> <!-- E.g., Playfield Left -->
    <NumberOfLedsStrip2>144</NumberOfLedsStrip2> <!-- E.g., Playfield Right -->
    <NumberOfLedsStrip3>256</NumberOfLedsStrip3> <!-- E.g., Matrix -->
    <NumberOfLedsStrip4>0</NumberOfLedsStrip4>
    <NumberOfLedsStrip5>0</NumberOfLedsStrip5>
    <!-- ... unused strips up to 16 can be omitted ... -->
  </FastUdpController>
</OutputControllers>
```
