#pragma once
#include <Arduino.h>

// Non-blocking WiFi: the light keeps working while offline or in setup portal.
void networkBegin();
void networkLoop(uint32_t now);
bool networkConnected();
void networkRestartMdns();
void networkResetCredentials();
