#include "settings.h"
#include <Preferences.h>
#include "config.h"

Settings settings;

static Preferences prefs;
static const char *NS_CFG = "cfg";
static const char *NS_STATE = "ledbar";  // phase 1 namespace, kept for upgrade compatibility

void settingsDefaults(Settings &s) {
  // eFuse MAC works before WiFi is started; bytes are little-endian.
  uint64_t mac = ESP.getEfuseMac();
  snprintf(s.deviceName, sizeof(s.deviceName), "ledbar-%02x%02x",
           (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  s.swapPads = false;
  s.tapMode = TapMode::OnOff;
  s.stepPct = DEF_STEP_PCT;
  s.rampMs = DEF_RAMP_MS;
  s.minPct = DEF_MIN_PCT;
  s.maxPct = DEF_MAX_PCT;
  s.onLevelMode = OnLevelMode::Last;
  s.fixedOnPct = DEF_FIXED_ON_PCT;
}

bool isValidHostname(const char *name) {
  size_t n = strlen(name);
  if (n == 0 || n > 31 || !isalpha((unsigned char)name[0])) return false;
  for (size_t i = 0; i < n; i++) {
    char c = name[i];
    if (!isalnum((unsigned char)c) && c != '-') return false;
  }
  return name[n - 1] != '-';
}

void settingsClamp(Settings &s) {
  s.stepPct = constrain(s.stepPct, 1, 50);
  s.rampMs = constrain(s.rampMs, MIN_RAMP_MS, MAX_RAMP_MS);
  s.minPct = constrain(s.minPct, 1, 100);
  s.maxPct = constrain(s.maxPct, s.minPct, 100);
  s.fixedOnPct = constrain(s.fixedOnPct, s.minPct, s.maxPct);
  if ((uint8_t)s.tapMode > 1) s.tapMode = TapMode::OnOff;
  if ((uint8_t)s.onLevelMode > 1) s.onLevelMode = OnLevelMode::Last;
  if (!isValidHostname(s.deviceName)) {
    Settings d;
    settingsDefaults(d);
    strlcpy(s.deviceName, d.deviceName, sizeof(s.deviceName));
  }
}

void settingsLoad() {
  settingsDefaults(settings);
  prefs.begin(NS_CFG, true);
  String name = prefs.getString("name", settings.deviceName);
  strlcpy(settings.deviceName, name.c_str(), sizeof(settings.deviceName));
  settings.swapPads = prefs.getBool("swap", settings.swapPads);
  settings.tapMode = (TapMode)prefs.getUChar("tapMode", (uint8_t)settings.tapMode);
  settings.stepPct = prefs.getUChar("step", settings.stepPct);
  settings.rampMs = prefs.getUShort("ramp", settings.rampMs);
  settings.minPct = prefs.getUChar("min", settings.minPct);
  settings.maxPct = prefs.getUChar("max", settings.maxPct);
  settings.onLevelMode = (OnLevelMode)prefs.getUChar("onMode", (uint8_t)settings.onLevelMode);
  settings.fixedOnPct = prefs.getUChar("onPct", settings.fixedOnPct);
  prefs.end();
  settingsClamp(settings);
}

void settingsSave() {
  prefs.begin(NS_CFG, false);
  prefs.putString("name", settings.deviceName);
  prefs.putBool("swap", settings.swapPads);
  prefs.putUChar("tapMode", (uint8_t)settings.tapMode);
  prefs.putUChar("step", settings.stepPct);
  prefs.putUShort("ramp", settings.rampMs);
  prefs.putUChar("min", settings.minPct);
  prefs.putUChar("max", settings.maxPct);
  prefs.putUChar("onMode", (uint8_t)settings.onLevelMode);
  prefs.putUChar("onPct", settings.fixedOnPct);
  prefs.end();
}

void stateLoad(bool &on, uint16_t &level) {
  prefs.begin(NS_STATE, true);
  on = prefs.getBool("on", true);
  level = prefs.getUShort("level", DEFAULT_LEVEL);
  prefs.end();
}

void stateSave(bool on, uint16_t level) {
  prefs.begin(NS_STATE, false);
  if (prefs.getBool("on", !on) != on) prefs.putBool("on", on);
  if (prefs.getUShort("level", level + 1) != level) prefs.putUShort("level", level);
  prefs.end();
}

void settingsFactoryReset() {
  prefs.begin(NS_CFG, false);
  prefs.clear();
  prefs.end();
  prefs.begin(NS_STATE, false);
  prefs.clear();
  prefs.end();
}
