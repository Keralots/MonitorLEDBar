#pragma once
#include <Arduino.h>

enum class TapMode : uint8_t { OnOff = 0, Step = 1 };
enum class OnLevelMode : uint8_t { Last = 0, Fixed = 1 };

struct Settings {
  char deviceName[32];
  bool swapPads;
  TapMode tapMode;
  uint8_t stepPct;
  uint16_t rampMs;
  uint8_t minPct;
  uint8_t maxPct;
  OnLevelMode onLevelMode;
  uint8_t fixedOnPct;
  bool mqttEnabled;
  char mqttHost[64];
  uint16_t mqttPort;
  char mqttUser[64];
  char mqttPass[64];
};

extern Settings settings;

void settingsLoad();
void settingsSave();
void settingsClamp(Settings &s);
void settingsDefaults(Settings &s);
bool isValidHostname(const char *name);

// Light state is stored separately; it changes far more often than settings.
void stateLoad(bool &on, uint16_t &level);
void stateSave(bool on, uint16_t level);

void settingsFactoryReset();
