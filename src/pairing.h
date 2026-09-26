#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Peer-to-peer bar groups over ESP-NOW: every member mirrors on/off and level,
// so pads, web or Home Assistant on any bar drive the whole group.
void pairBegin();
void pairLoop(uint32_t now);
void pairFillJson(JsonDocument &doc);
bool pairAdd(const uint8_t mac[6]);
bool pairRemove(const uint8_t mac[6]);
void pairLeave();
bool parseMac(const char *s, uint8_t mac[6]);
