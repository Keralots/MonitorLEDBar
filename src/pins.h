#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// GPIOs broken out on the ESP32-C3 SuperMini header, with the reason a pin is unusable.
struct PinInfo {
  uint8_t gpio;
  bool usable;
  const char *note;
};

bool pinUsable(uint8_t gpio);
void pinsFillJson(JsonArray arr);
