#include "led_controller.h"
#include "config.h"
#include "driver/gpio.h"

void LedController::begin(bool on, uint16_t level) {
  on_ = on;
  level_ = (level >= 1 && level <= LEVEL_MAX) ? level : DEFAULT_LEVEL;

  ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
  ledcAttachPin(PIN_LED_PWM, PWM_CHANNEL);
  // Max pad drive current for faster MOSFET gate edges; set after attach.
  gpio_set_drive_capability((gpio_num_t)PIN_LED_PWM, GPIO_DRIVE_CAP_3);
  writeOutput(0);
  lastUpdate_ = millis();
}

void LedController::update(uint32_t now) {
  float dt = (float)(now - lastUpdate_);
  lastUpdate_ = now;

  if (rampDir_ != 0) {
    level_ += rampDir_ * dt * LEVEL_MAX / RAMP_FULL_MS;
    if (level_ > LEVEL_MAX) level_ = LEVEL_MAX;
    if (level_ <= 0) {
      // Dimmed to zero: turn off, keep the pre-hold level for the next turnOn().
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
  if (level_ < 1) level_ = DEFAULT_LEVEL;
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
    level_ = 0;
  }
  rampStartLevel_ = level_ >= 1 ? level_ : DEFAULT_LEVEL;
  rampDir_ = dir;
}

void LedController::stopRamp(int8_t dir) {
  if (rampDir_ != dir) return;
  rampDir_ = 0;
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
