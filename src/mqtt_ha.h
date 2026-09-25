#pragma once

// Home Assistant integration: MQTT device discovery, runs in its own task so a
// slow or missing broker never blocks the touch pads.
void mqttBegin();
void mqttLoop();           // main loop: applies HA commands, flags state changes
void mqttConfigChanged();  // call after settings are saved
void mqttRemoveDevice();   // factory reset: remove the device from HA (blocks up to ~1.5 s)
const char *mqttStatus();  // "disabled", "connecting" or "connected"
const char *mqttError();   // last connection problem in plain words, "" if none
