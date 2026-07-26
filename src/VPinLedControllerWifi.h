#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>
#include "VPinNeoPixelBus/VPinNeoPixelBus.h"
#include <Preferences.h>
#include <nvs_flash.h>
Preferences preferences;
Stream* activeSerial = nullptr;
RTC_NOINIT_ATTR uint32_t resetMagicNumber;