#pragma once
#include "VPinEthernet/VPinEthernet.h"
#include "VPinEthernet/EthernetUdp.h"
#include "VPinNeoPixelBus/VPinNeoPixelBus.h"
#include <nvs_flash.h>
#include <Preferences.h>
Stream* activeSerial = nullptr;
Preferences preferences;
RTC_NOINIT_ATTR uint32_t resetMagicNumber;