#pragma once
#include <Arduino.h>
#include <functional>

// Brightness state and PWM output. Levels are perceptual, 0..LEVEL_MAX.
class LedController {
public:
  using ChangeListener = std::function<void()>;

  void begin(bool on, uint16_t level);
  void update(uint32_t now);

  // minLevel..maxLevel bound every on-level; fixedOnLevel 0 = restore last level.
  void configure(uint16_t minLevel, uint16_t maxLevel, uint16_t fixedOnLevel, uint32_t rampMs);

  void turnOn();
  void turnOff();
  void startRamp(int8_t dir);  // +1 brighten, -1 dim
  void stopRamp(int8_t dir);
  void step(int8_t dir, uint16_t amount);
  void setLevel(uint16_t level);  // 0 turns off

  bool isOn() const { return on_; }
  uint16_t level() const { return (uint16_t)lroundf(level_); }  // level restored by turnOn()

  // Fires on settled changes only (not every ramp step).
  void setChangeListener(ChangeListener cb) { onChange_ = cb; }

private:
  float clampLevel(float level) const;
  void writeOutput(float level);
  void notify();

  bool on_ = false;
  float level_ = 0;
  float shown_ = 0;
  float rampStartLevel_ = 0;
  int8_t rampDir_ = 0;
  uint16_t minLevel_ = 1;
  uint16_t maxLevel_ = 1000;
  uint16_t fixedOnLevel_ = 0;
  uint32_t rampMs_ = 3000;
  uint32_t lastUpdate_ = 0;
  uint32_t lastDuty_ = UINT32_MAX;
  ChangeListener onChange_;
};
