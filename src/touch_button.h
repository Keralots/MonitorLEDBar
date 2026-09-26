#pragma once
#include <Arduino.h>

enum class TouchEvent : uint8_t { None, Tap, HoldStart, HoldEnd };

// TTP223 in default mode: momentary, output HIGH while touched.
class TouchButton {
public:
  void begin(uint8_t pin);
  TouchEvent update(uint32_t now);

private:
  uint8_t pin_ = 0;
  bool raw_ = false;
  bool stable_ = false;
  bool holding_ = false;
  uint32_t rawChangedAt_ = 0;
  uint32_t pressedAt_ = 0;
};
