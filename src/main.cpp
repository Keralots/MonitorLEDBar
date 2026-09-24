#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "led_controller.h"
#include "touch_button.h"

enum class Role : uint8_t { Down, Up };

// Pad-to-role mapping; the web UI will make this swappable.
struct Pad {
  TouchButton button;
  Role role;
};

static Pad pads[] = {
  {TouchButton(PIN_TOUCH_A), Role::Down},
  {TouchButton(PIN_TOUCH_B), Role::Up},
};

static LedController led;
static Preferences prefs;

static bool savePending = false;
static uint32_t changedAt = 0;
static bool savedOn = false;
static uint16_t savedLevel = 0;

static void handleEvent(Role role, TouchEvent ev) {
  int8_t dir = role == Role::Up ? 1 : -1;
  switch (ev) {
    case TouchEvent::Tap:
      if (role == Role::Up) led.turnOn();
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

static void saveIfDue(uint32_t now) {
  if (!savePending || now - changedAt < SAVE_DELAY_MS) return;
  savePending = false;
  if (led.isOn() != savedOn) {
    savedOn = led.isOn();
    prefs.putBool("on", savedOn);
  }
  if (led.level() != savedLevel) {
    savedLevel = led.level();
    prefs.putUShort("level", savedLevel);
  }
}

void setup() {
  Serial.begin(115200);

  prefs.begin("ledbar", false);
  savedOn = prefs.getBool("on", true);
  savedLevel = prefs.getUShort("level", DEFAULT_LEVEL);

  for (auto &p : pads) p.button.begin();

  led.begin(savedOn, savedLevel);
  led.setChangeListener([] {
    savePending = true;
    changedAt = millis();
    Serial.printf("on=%d level=%u\n", led.isOn(), led.level());
  });

  Serial.printf("MonitorLEDBar start: on=%d level=%u\n", savedOn, savedLevel);
}

void loop() {
  uint32_t now = millis();
  for (auto &p : pads) {
    TouchEvent ev = p.button.update(now);
    if (ev != TouchEvent::None) handleEvent(p.role, ev);
  }
  led.update(now);
  saveIfDue(now);
  delay(5);
}
