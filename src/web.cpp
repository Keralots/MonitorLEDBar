#include "web.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include "app.h"
#include "config.h"
#include "mqtt_ha.h"
#include "network.h"
#include "pairing.h"
#include "settings.h"
#include "web_pages.h"

static WebServer server(80);
static bool started = false;

static uint8_t levelToPct(uint16_t level) {
  return (uint8_t)((level + 5) / 10);
}

static void sendJson(const JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void sendOk() {
  JsonDocument doc;
  doc["success"] = true;
  sendJson(doc);
}

static void sendError(const char *msg) {
  JsonDocument doc;
  doc["success"] = false;
  doc["message"] = msg;
  sendJson(doc, 400);
}

static void sendAsset(const char *data, const char *type) {
  // Asset URLs carry ?v=<version>, so long caching is safe across updates.
  server.sendHeader("Cache-Control", "public, max-age=31536000");
  server.send_P(200, type, data);
}

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleConfig() {
  JsonDocument doc;
  doc["deviceName"] = settings.deviceName;
  doc["swapPads"] = settings.swapPads;
  doc["tapMode"] = (uint8_t)settings.tapMode;
  doc["stepPct"] = settings.stepPct;
  doc["rampMs"] = settings.rampMs;
  doc["minPct"] = settings.minPct;
  doc["maxPct"] = settings.maxPct;
  doc["onLevelMode"] = (uint8_t)settings.onLevelMode;
  doc["fixedOnPct"] = settings.fixedOnPct;
  doc["mqttEnabled"] = settings.mqttEnabled;
  doc["mqttHost"] = settings.mqttHost;
  doc["mqttPort"] = settings.mqttPort;
  doc["mqttUser"] = settings.mqttUser;
  doc["mqttPassSet"] = settings.mqttPass[0] != 0;  // the password itself never leaves the device
  sendJson(doc);
}

// Saturate before narrowing so out-of-range input clamps instead of wrapping.
static uint8_t argByte(const char *name) {
  return (uint8_t)constrain(server.arg(name).toInt(), 0, 255);
}

// Only fields present in the request are changed.
static void handleSave() {
  Settings s = settings;
  if (server.hasArg("deviceName")) {
    String name = server.arg("deviceName");
    name.trim();
    if (!isValidHostname(name.c_str())) return sendError("Invalid device name");
    strlcpy(s.deviceName, name.c_str(), sizeof(s.deviceName));
  }
  if (server.hasArg("swapPads")) s.swapPads = server.arg("swapPads") == "1";
  if (server.hasArg("tapMode")) s.tapMode = (TapMode)argByte("tapMode");
  if (server.hasArg("stepPct")) s.stepPct = argByte("stepPct");
  if (server.hasArg("rampMs")) s.rampMs = constrain(server.arg("rampMs").toInt(), 0, 65535);
  if (server.hasArg("minPct")) s.minPct = argByte("minPct");
  if (server.hasArg("maxPct")) s.maxPct = argByte("maxPct");
  if (server.hasArg("onLevelMode")) s.onLevelMode = (OnLevelMode)argByte("onLevelMode");
  if (server.hasArg("fixedOnPct")) s.fixedOnPct = argByte("fixedOnPct");
  if (server.hasArg("mqttEnabled")) s.mqttEnabled = server.arg("mqttEnabled") == "1";
  if (server.hasArg("mqttHost")) {
    String host = server.arg("mqttHost");
    host.trim();
    if (host.length() >= sizeof(s.mqttHost)) return sendError("MQTT host too long");
    strlcpy(s.mqttHost, host.c_str(), sizeof(s.mqttHost));
  }
  if (server.hasArg("mqttPort")) s.mqttPort = constrain(server.arg("mqttPort").toInt(), 1, 65535);
  if (server.hasArg("mqttUser")) {
    if (server.arg("mqttUser").length() >= sizeof(s.mqttUser)) return sendError("MQTT user too long");
    strlcpy(s.mqttUser, server.arg("mqttUser").c_str(), sizeof(s.mqttUser));
  }
  // Empty password field keeps the stored one; clearing the user clears both.
  if (server.hasArg("mqttPass") && server.arg("mqttPass").length()) {
    if (server.arg("mqttPass").length() >= sizeof(s.mqttPass)) return sendError("MQTT password too long");
    strlcpy(s.mqttPass, server.arg("mqttPass").c_str(), sizeof(s.mqttPass));
  }
  if (s.mqttUser[0] == 0) s.mqttPass[0] = 0;
  if (s.mqttEnabled && s.mqttHost[0] == 0) return sendError("MQTT host is required");
  settingsClamp(s);

  bool nameChanged = strcmp(s.deviceName, settings.deviceName) != 0;
  bool mqttChanged = nameChanged || s.mqttEnabled != settings.mqttEnabled || s.mqttPort != settings.mqttPort ||
                     strcmp(s.mqttHost, settings.mqttHost) || strcmp(s.mqttUser, settings.mqttUser) ||
                     strcmp(s.mqttPass, settings.mqttPass);
  settings = s;
  settingsSave();
  applySettings();
  if (mqttChanged) mqttConfigChanged();
  if (nameChanged) {
    WiFi.setHostname(settings.deviceName);
    networkRestartMdns();
  }
  sendOk();
}

static void handleState() {
  JsonDocument doc;
  doc["on"] = led.isOn();
  doc["level"] = levelToPct(led.level());
  sendJson(doc);
}

static void handleLight() {
  if (server.hasArg("level")) {
    int pct = constrain(server.arg("level").toInt(), 0, 100);
    led.setLevel(pct * 10);
  } else if (server.hasArg("on")) {
    if (server.arg("on") == "1") led.turnOn();
    else led.turnOff();
  }
  handleState();
}

static void handleInfo() {
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  doc["ip"] = WiFi.localIP().toString();
  doc["hostname"] = settings.deviceName;
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["mqtt"] = mqttStatus();
  doc["mqttError"] = mqttError();
  sendJson(doc);
}

static void handlePairInfo() {
  JsonDocument doc;
  pairFillJson(doc);
  sendJson(doc);
}

static bool macArg(uint8_t mac[6]) {
  return server.hasArg("mac") && parseMac(server.arg("mac").c_str(), mac);
}

static void handlePairAdd() {
  uint8_t mac[6];
  if (!macArg(mac)) return sendError("Invalid MAC address");
  if (!pairAdd(mac)) return sendError("Cannot pair this bar (already paired or group full)");
  sendOk();
}

static void handlePairRemove() {
  uint8_t mac[6];
  if (!macArg(mac)) return sendError("Invalid MAC address");
  if (!pairRemove(mac)) return sendError("Not a member of this group");
  sendOk();
}

static void handlePairLeave() {
  pairLeave();
  sendOk();
}

static void handleReboot() {
  sendOk();
  delay(500);
  ESP.restart();
}

static void handleWifiReset() {
  sendOk();
  delay(500);
  networkResetCredentials();
  ESP.restart();
}

static void handleFactoryReset() {
  sendOk();
  delay(500);
  mqttRemoveDevice();
  pairLeave();
  settingsFactoryReset();
  networkResetCredentials();
  ESP.restart();
}

static void handleUpdateDone() {
  if (Update.hasError()) {
    server.send(500, "text/plain", String("Update failed: ") + Update.errorString());
    return;
  }
  server.send(200, "text/plain", "OK");
  delay(1000);
  ESP.restart();
}

static void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("OTA: %u bytes written\n", upload.totalSize);
    else Update.printError(Serial);
  }
}

void webBegin() {
  if (started) return;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/portal.css", HTTP_GET, [] { sendAsset(PORTAL_CSS, "text/css"); });
  server.on("/portal.js", HTTP_GET, [] { sendAsset(PORTAL_JS, "application/javascript"); });
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/light", HTTP_POST, handleLight);
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/pair", HTTP_GET, handlePairInfo);
  server.on("/api/pair/add", HTTP_POST, handlePairAdd);
  server.on("/api/pair/remove", HTTP_POST, handlePairRemove);
  server.on("/api/pair/leave", HTTP_POST, handlePairLeave);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/wifireset", HTTP_POST, handleWifiReset);
  server.on("/api/reset", HTTP_POST, handleFactoryReset);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
  started = true;
  Serial.println("Web: server started");
}

void webLoop() {
  if (started) server.handleClient();
}
