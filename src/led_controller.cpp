#include "led_controller.h"
#include "config.h"
#include "driver/gpio.h"

void LedController::begin(uint8_t pwmPin, bool on, uint16_t level) {
  on_ = on;
  level_ = (level >= 1 && level <= LEVEL_MAX) ? level : DEFAULT_LEVEL;
  if (on_ && fixedOnLevel_) level_ = fixedOnLevel_;
  level_ = clampLevel(level_);

  ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
  ledcAttachPin(pwmPin, PWM_CHANNEL);
  // Max pad drive current for faster MOSFET gate edges; set after attach.
  gpio_set_drive_capability((gpio_num_t)pwmPin, GPIO_DRIVE_CAP_3);
  writeOutput(0);
  lastUpdate_ = millis();
}

void LedController::configure(uint16_t minLevel, uint16_t maxLevel, uint16_t fixedOnLevel, uint32_t rampMs) {
  minLevel_ = constrain(minLevel, 1, LEVEL_MAX);
  maxLevel_ = constrain(maxLevel, minLevel_, LEVEL_MAX);
  fixedOnLevel_ = fixedOnLevel ? constrain(fixedOnLevel, minLevel_, maxLevel_) : 0;
  rampMs_ = rampMs;
  float clamped = clampLevel(level_);
  if (clamped != level_) {
    level_ = clamped;
    notify();
  }
}

float LedController::clampLevel(float level) const {
  return constrain(level, (float)minLevel_, (float)maxLevel_);
}

void LedController::update(uint32_t now) {
  float dt = (float)(now - lastUpdate_);
  lastUpdate_ = now;

  if (rampDir_ != 0) {
    level_ += rampDir_ * dt * LEVEL_MAX / rampMs_;
    if (level_ > maxLevel_) level_ = maxLevel_;
    if (level_ < minLevel_) {
      // Dimmed past the minimum: turn off, keep the pre-hold level for the next turnOn().
      level_ = rampStartLevel_;
      on_ = false;
      rampDir_ = 0;
      notify();
    }
  }

  float target = on_ ? level_ : 0;
  float step = dt * LEVEL_MAX / FADE_FULL_MS;
  if (shown_ < target) shown_ = min(shown_ + step, target);
  else if (shown_ > target) shown_ = max(shown_ - step, target);
  writeOutput(shown_);
}

void LedController::turnOn() {
  if (on_) return;
  on_ = true;
  level_ = clampLevel(fixedOnLevel_ ? fixedOnLevel_ : level_);
  notify();
}

void LedController::turnOff() {
  if (!on_) return;
  on_ = false;
  rampDir_ = 0;
  notify();
}

void LedController::startRamp(int8_t dir) {
  if (dir < 0 && !on_) return;
  if (dir > 0 && !on_) {
    on_ = true;
    level_ = minLevel_;
  }
  rampStartLevel_ = level_;
  rampDir_ = dir;
}

void LedController::stopRamp(int8_t dir) {
  if (rampDir_ != dir) return;
  rampDir_ = 0;
  notify();
}

void LedController::step(int8_t dir, uint16_t amount) {
  if (!on_) {
    if (dir > 0) turnOn();
    return;
  }
  float next = level_ + dir * (float)amount;
  // Stepping below the minimum turns off and keeps the current level for turnOn().
  if (next < minLevel_) {
    turnOff();
    return;
  }
  level_ = clampLevel(next);
  notify();
}

void LedController::setLevel(uint16_t level) {
  if (level == 0) {
    turnOff();
    return;
  }
  rampDir_ = 0;
  on_ = true;
  level_ = clampLevel(level);
  notify();
}

void LedController::setState(bool on, uint16_t level) {
  float next = level ? clampLevel(level) : level_;
  if (on == on_ && next == level_ && rampDir_ == 0) return;
  rampDir_ = 0;
  on_ = on;
  level_ = next;
  notify();
}

void LedController::writeOutput(float level) {
  const uint32_t maxDuty = (1u << PWM_BITS) - 1;
  uint32_t duty = 0;
  // Any non-zero level maps to at least duty 1 so "on" never goes dark.
  if (level > 0.5f) duty = 1 + (uint32_t)lroundf(powf(level / LEVEL_MAX, GAMMA) * (maxDuty - 1));
  if (duty == lastDuty_) return;
  lastDuty_ = duty;
  ledcWrite(PWM_CHANNEL, duty);
}

void LedController::notify() {
  if (onChange_) onChange_();
}
