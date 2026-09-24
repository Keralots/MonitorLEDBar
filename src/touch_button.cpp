#include "touch_button.h"
#include "config.h"

void TouchButton::begin() {
  pinMode(pin_, INPUT_PULLDOWN);
}

TouchEvent TouchButton::update(uint32_t now) {
  bool r = digitalRead(pin_) == HIGH;
  if (r != raw_) {
    raw_ = r;
    rawChangedAt_ = now;
  }

  if (raw_ != stable_ && now - rawChangedAt_ >= DEBOUNCE_MS) {
    stable_ = raw_;
    if (stable_) {
      pressedAt_ = now;
      return TouchEvent::None;
    }
    if (holding_) {
      holding_ = false;
      return TouchEvent::HoldEnd;
    }
    return TouchEvent::Tap;
  }

  if (stable_ && !holding_ && now - pressedAt_ >= HOLD_THRESHOLD_MS) {
    holding_ = true;
    return TouchEvent::HoldStart;
  }
  return TouchEvent::None;
}
