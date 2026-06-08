# 🔌 Ethernet Configuration (W5500 LAN / UDP)

**Recommended for: Uncompromising high-end setups, massive LED matrices, maximum FPS (110+).**

This setup utilizes an external **W5500 Ethernet module** connected to the ESP32-S3 via SPI. It provides a rock-solid, hardwired UDP data stream directly from your Virtual Pinball PC. This completely eliminates the USB bottleneck and avoids potential wireless interference, making it the ultimate solution for extreme framerates.

---

## 🚀 The "Auto-Config" Magic (Zero Code Editing)

Just like our WiFi protocol, the custom C# DirectOutput network plugin transmits a "Layout Header" at the start of every UDP frame over the LAN cable. 

**What this means for you:** You never have to configure or hardcode any LED strip lengths inside the Arduino sketch! The ESP32-S3 dynamically reads the strip lengths directly from the incoming DOF signal and automatically allocates the correct DMA memory in the background. If you change your layout in DOF, the ESP32 adapts instantly on the fly.

---

## 🌐 Network Topology & Physical Wiring

To guarantee zero latency, this setup requires a dedicated, isolated network connection. **Do NOT connect the ESP32 to your standard home internet router via DHCP!** Most VPin cabinets are played offline anyway, and routing a 100+ FPS uncompressed UDP stream through a busy home router will cause jitter and packet drops.

Choose one of these two recommended connection methods:

### Option A: Direct Connection (PC to ESP32)
If your PC has a spare Ethernet port (or if you use a cheap USB-to-LAN adapter), you can connect the PC directly to the W5500 module.
* Use a **Crossover Cable** (though most modern PC network cards support Auto-MDIX and will work perfectly with a standard patch cable).

### Option B: Dedicated VPin Switch (Highly Recommended)
If your cabinet features other local network devices that need to communicate with your PC (e.g., an LG TV using the LG Companion App for auto-power-on), a direct connection isn't enough.
* Buy a cheap, unmanaged 5-Port Gigabit or Fast-Ethernet Switch (usually available for under $15).
* Connect your PC, your LG TV, and the W5500 ESP32 module to this switch using standard Ethernet patch cables.

---

## 🛠️ Step 1: Setting a Static IP in Windows

Because we are bypassing standard home routers, there is no DHCP server to assign IP addresses. You must manually assign a **Static IP** to the network adapter on your VPin PC.

By default, the sketch assigns the IP `192.168.1.201` to the ESP32. We will set the PC to `192.168.1.100`.

1. Press `Win + R`, type `ncpa.cpl`, and hit Enter.
2. Right-click the Ethernet adapter connected to the ESP32/Switch and select **Properties**.
3. Select **Internet Protocol Version 4 (TCP/IPv4)** and click **Properties**.
4. Check **"Use the following IP address"** and enter:
   * **IP address:** `192.168.1.100`
   * **Subnet mask:** `255.255.255.0`
   * **Default gateway:** *(Leave completely blank!)*
5. Click OK to save.

---

## 💻 Step 2: Arduino IDE Settings (ESP32-S3)

Before flashing the Ethernet sketch, ensure your Arduino IDE `Tools` menu is configured correctly. 

**Crucial Setting:** You must set `USB CDC On Boot` to **Disabled**! Leaving it enabled while using LAN can cause the ESP32 to freeze if no serial monitor is actively listening via USB.

**Key Settings Checklist:**
* **Board:** ESP32S3 Dev Module
* **USB CDC On Boot:** `Disabled` *(CRITICAL!)*
* **USB Mode:** `Hardware CDC and JTAG`
* **Upload Mode:** `UART0 / Hardware CDC`
* **CPU Frequency:** `240MHz`
* **Flash Mode:** `QIO 80MHz`

---

## 🛡️ Step 3: Windows Firewall Settings

Windows aggressively blocks outgoing UDP traffic on static, unidentified networks. You must allow DOF to send data through the firewall.

1. Open **Windows Defender Firewall with Advanced Security**.
2. Create a new **Outbound Rule**.
3. Select **Port** -> **UDP**.
4. Specify the local and remote port: **`6454`** *(This is the default Art-Net/DOF port)*.
5. Allow the connection for all network profiles (Domain, Private, Public).

---

## ⚙️ Step 4: DOF Configuration & Custom DLLs

⚠️ **IMPORTANT: Custom DOF DLL Replacement Required!**
Because our `FastUdpController` is currently pending a Pull Request and is not yet included in the official DirectOutput release, you **must** replace your existing DOF DLLs with the modified ones provided in this package.

1. Locate the **DOF_x86** and **DOF_x64** folders provided in this project download.
2. Navigate to your existing DirectOutput installation directory on your PC.
3. Replace the original DLL files in your local `x86` and `x64` folders with our provided versions.
*(Note: Our DLLs are compiled directly from the latest official mjr source code branch. You won't lose any existing features!)*

After replacing the DLLs, add the custom controller to your `cabinet.xml`. You only need to define the strips you actually use.

```xml
<OutputControllers>
  <FastUdpController>
    <Name>ESP32_Ethernet_Controller</Name>
    <IpAddress>192.168.1.201</IpAddress> <!-- Default IP of the W5500 Sketch -->
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
