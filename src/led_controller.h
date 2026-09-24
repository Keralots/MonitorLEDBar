#pragma once
#include <Arduino.h>
#include <functional>

// Brightness state and PWM output. Levels are perceptual, 0..LEVEL_MAX.
class LedController {
public:
  using ChangeListener = std::function<void()>;

  void begin(bool on, uint16_t level);
  void update(uint32_t now);

  void turnOn();
  void turnOff();
  void startRamp(int8_t dir);  // +1 brighten, -1 dim
  void stopRamp(int8_t dir);

  bool isOn() const { return on_; }
  uint16_t level() const { return (uint16_t)lroundf(level_); }  // level restored by turnOn()

  // Fires on settled changes only (not every ramp step).
  void setChangeListener(ChangeListener cb) { onChange_ = cb; }

private:
  void writeOutput(float level);
  void notify();

  bool on_ = false;
  float level_ = 0;
  float shown_ = 0;
  float rampStartLevel_ = 0;
  int8_t rampDir_ = 0;
  uint32_t lastUpdate_ = 0;
  uint32_t lastDuty_ = UINT32_MAX;
  ChangeListener onChange_;
};
