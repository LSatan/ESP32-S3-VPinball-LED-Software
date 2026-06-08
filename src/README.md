# 🛠️ Custom Libraries (src)

⚠️ **IMPORTANT NOTICE:** The libraries inside this folder have been **heavily modified** to achieve maximum performance and frame stability (>100 FPS) on the ESP32-S3 in conjunction with DirectOutput (DOF). 
**Please NEVER update these libraries via the Arduino Library Manager!** An update will overwrite the hardware-level patches and lead to system crashes or massive performance drops.

---

## 1. VPinEthernet (Modified Ethernet Library)
This library is based on the original Arduino Ethernet library for the W5500 chip, but has been radically stripped down.

* **Original Project:** [Arduino Ethernet](https://github.com/arduino-libraries/Ethernet)
* **Changes for VPin use:**
  * **RAM Optimization:** All TCP overhead, DHCP, Server, and Client classes have been completely removed. The library essentially consists only of the pure `EthernetUdp` class.
  * **Speed:** All blocking `delay()` calls during hardware initialization have been removed.
  * **Result:** The ESP32 saves massive amounts of RAM, which is now available for the UDP buffer and LED DMA allocations. Furthermore, the controller boots up without any delay.

## 2. NeoPixelBusVPin (Modified NeoPixelBus Library)
This is a heavily customized version of the brilliant NeoPixelBus library by Makuna, specifically optimized for parallel 16-channel operation (`NeoEsp32LcdX16Ws2812xMethod`).

* **Original Project:** [NeoPixelBus by Makuna](https://github.com/Makuna/NeoPixelBus)
* **Changes for VPin use:**
  * **DMA Memory Leak Patch:** The original library does not free the internal i2s/LCD DMA memory when an LED object is destroyed via `delete`. Changing the table layout dynamically via DOF previously led to creeping RAM fragmentation and eventually a `StoreProhibited` Core Panic.
  * **The Patch:** Proper destructor handling for the LCD method was implemented, returning the memory back to the ESP32 operating system. 
  * **Result:** The VPin layout (the lengths of the individual LED strips) can now be changed dynamically on the fly during runtime (e.g., when switching tables in PinUP Popper) without having to restart the ESP32 or running out of RAM.
