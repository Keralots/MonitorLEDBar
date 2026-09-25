#include <Arduino.h>
#include "app.h"
#include "config.h"
#include "network.h"
#include "settings.h"
#include "touch_button.h"
#include "web.h"

enum class Role : uint8_t { Down, Up };

struct Pad {
  TouchButton button;
  bool isPadA;
  Role heldRole;  // latched at HoldStart so a mid-hold swap cannot orphan the ramp
};

static Pad pads[] = {
  {TouchButton(PIN_TOUCH_A), true, Role::Down},
  {TouchButton(PIN_TOUCH_B), false, Role::Up},
};

LedController led;

static bool savePending = false;
static uint32_t changedAt = 0;

void applySettings() {
  uint16_t fixed = settings.onLevelMode == OnLevelMode::Fixed ? settings.fixedOnPct * 10 : 0;
  led.configure(settings.minPct * 10, settings.maxPct * 10, fixed, settings.rampMs);
}

static Role roleOf(bool isPadA) {
  // Pad A dims by default; swapPads flips both pads.
  return (isPadA != settings.swapPads) ? Role::Down : Role::Up;
}

static void handleEvent(Pad &pad, TouchEvent ev) {
  if (ev == TouchEvent::HoldStart) pad.heldRole = roleOf(pad.isPadA);
  Role role = ev == TouchEvent::HoldEnd ? pad.heldRole : roleOf(pad.isPadA);
  int8_t dir = role == Role::Up ? 1 : -1;
  switch (ev) {
    case TouchEvent::Tap:
      if (settings.tapMode == TapMode::Step) led.step(dir, settings.stepPct * 10);
      else if (role == Role::Up) led.turnOn();
      else led.turnOff();
      break;
    case TouchEvent::HoldStart:
      led.startRamp(dir);
      break;
    case TouchEvent::HoldEnd:
      led.stopRamp(dir);
      break;
    default:
      break;
  }
}

static void saveStateIfDue(uint32_t now) {
  if (!savePending || now - changedAt < SAVE_DELAY_MS) return;
  savePending = false;
  stateSave(led.isOn(), led.level());
}

void setup() {
  Serial.begin(115200);

  settingsLoad();
  bool on;
  uint16_t level;
  stateLoad(on, level);

  for (auto &p : pads) p.button.begin();

  applySettings();
  led.begin(on, level);
  led.setChangeListener([] {
    savePending = true;
    changedAt = millis();
    Serial.printf("on=%d level=%u\n", led.isOn(), led.level());
  });

  Serial.printf("MonitorLEDBar v%s start: on=%d level=%u name=%s\n", FW_VERSION, on, level, settings.deviceName);
  networkBegin();
}

void loop() {
  uint32_t now = millis();
  for (auto &p : pads) {
    TouchEvent ev = p.button.update(now);
    if (ev != TouchEvent::None) handleEvent(p, ev);
  }

  led.update(now);
  saveStateIfDue(now);
  networkLoop(now);
  webLoop();
  delay(2);
}
