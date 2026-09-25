#include "mqtt_ha.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "app.h"
#include "config.h"
#include "settings.h"

namespace {

struct MqttConfig {
  bool enabled;
  char host[64];
  uint16_t port;
  char user[64];
  char pass[64];
  char name[32];
};

struct Command {
  int8_t on;     // -1 not set, 0 off, 1 on
  int8_t level;  // -1 not set, 0..100
};

const char *HA_STATUS_TOPIC = "homeassistant/status";
constexpr uint32_t STATE_MIN_INTERVAL_MS = 250;  // throttles state during a ramp

WiFiClient net;
PubSubClient client(net);
SemaphoreHandle_t cfgLock;
QueueHandle_t cmdQueue;
MqttConfig sharedCfg;  // written by mqttConfigChanged() under cfgLock

volatile uint32_t cfgGen = 0;
volatile bool stateDirty = true;
volatile bool lightOn = false;
volatile uint8_t lightPct = 0;
volatile bool rediscover = false;
volatile bool removeRequested = false;
volatile bool removeDone = false;
volatile uint8_t status = 0;  // 0 disabled, 1 connecting, 2 connected
constexpr int8_t ERR_NONE = 0, ERR_DNS = 100;
volatile int8_t lastError = ERR_NONE;  // ERR_DNS or a PubSubClient state code

char nodeId[24];
char topicState[48], topicSet[48], topicAvail[48], topicDiag[48], topicConfig[64];

void publishJson(const char *topic, const JsonDocument &doc, bool retain) {
  String out;
  serializeJson(doc, out);
  client.publish(topic, out.c_str(), retain);
}

void publishDiscovery(const MqttConfig &c) {
  JsonDocument doc;
  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"] = nodeId;
  dev["name"] = c.name;
  dev["mf"] = "DIY";
  dev["mdl"] = "MonitorLEDBar";
  dev["sw"] = FW_VERSION;
  dev["cu"] = String("http://") + WiFi.localIP().toString() + "/";

  JsonObject o = doc["o"].to<JsonObject>();
  o["name"] = "MonitorLEDBar";
  o["sw"] = FW_VERSION;

  JsonObject cmps = doc["cmps"].to<JsonObject>();

  JsonObject light = cmps["light"].to<JsonObject>();
  light["p"] = "light";
  light["name"] = nullptr;  // main feature: entity takes the device name
  light["unique_id"] = String(nodeId) + "_light";
  light["schema"] = "json";
  light["command_topic"] = topicSet;
  light["state_topic"] = topicState;
  light["availability_topic"] = topicAvail;
  light["brightness_scale"] = 100;
  light["supported_color_modes"].to<JsonArray>().add("brightness");

  JsonObject rssi = cmps["rssi"].to<JsonObject>();
  rssi["p"] = "sensor";
  rssi["name"] = "WiFi signal";
  rssi["unique_id"] = String(nodeId) + "_rssi";
  rssi["state_topic"] = topicDiag;
  rssi["availability_topic"] = topicAvail;
  rssi["value_template"] = "{{ value_json.rssi }}";
  rssi["device_class"] = "signal_strength";
  rssi["unit_of_measurement"] = "dBm";
  rssi["state_class"] = "measurement";
  rssi["entity_category"] = "diagnostic";

  JsonObject ip = cmps["ip"].to<JsonObject>();
  ip["p"] = "sensor";
  ip["name"] = "IP address";
  ip["unique_id"] = String(nodeId) + "_ip";
  ip["state_topic"] = topicDiag;
  ip["availability_topic"] = topicAvail;
  ip["value_template"] = "{{ value_json.ip }}";
  ip["icon"] = "mdi:ip-network";
  ip["entity_category"] = "diagnostic";

  // Not retained: HA's birth message triggers a resend, which avoids ghost devices.
  publishJson(topicConfig, doc, false);
}

void publishState() {
  JsonDocument doc;
  doc["state"] = lightOn ? "ON" : "OFF";
  doc["brightness"] = lightPct;
  doc["color_mode"] = "brightness";
  publishJson(topicState, doc, true);
}

void publishDiag() {
  JsonDocument doc;
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  publishJson(topicDiag, doc, true);
}

void onMessage(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, HA_STATUS_TOPIC) == 0) {
    if (length == 6 && memcmp(payload, "online", 6) == 0) rediscover = true;
    return;
  }
  if (strcmp(topic, topicSet) != 0) return;

  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  Command cmd{-1, -1};
  const char *state = doc["state"];
  if (state) cmd.on = strcmp(state, "ON") == 0 ? 1 : 0;
  if (doc["brightness"].is<int>()) cmd.level = constrain(doc["brightness"].as<int>(), 0, 100);
  xQueueSend(cmdQueue, &cmd, 0);
}

bool connectBroker(const MqttConfig &c) {
  // Resolve here so a bad hostname is reported separately from an unreachable broker.
  IPAddress ip;
  if (!ip.fromString(c.host) && !WiFi.hostByName(c.host, ip)) {
    Serial.printf("MQTT: cannot resolve %s\n", c.host);
    lastError = ERR_DNS;
    return false;
  }
  client.setServer(ip, c.port);
  const char *user = c.user[0] ? c.user : nullptr;
  const char *pass = c.user[0] ? c.pass : nullptr;
  Serial.printf("MQTT: connecting to %s:%u\n", c.host, c.port);
  if (!client.connect(nodeId, user, pass, topicAvail, 0, true, "offline")) {
    Serial.printf("MQTT: connect failed, rc=%d\n", client.state());
    lastError = client.state();
    return false;
  }
  Serial.println("MQTT: connected");
  lastError = ERR_NONE;
  client.subscribe(topicSet);
  client.subscribe(HA_STATUS_TOPIC);
  publishDiscovery(c);
  client.publish(topicAvail, "online", true);
  publishState();
  publishDiag();
  return true;
}

// Removes the device from HA and marks it offline.
void removeFromHa() {
  client.publish(topicConfig, "", false);
  client.publish(topicAvail, "offline", true);
  client.disconnect();
}

void mqttTask(void *) {
  MqttConfig cfg{};
  uint32_t seenGen = UINT32_MAX;
  uint32_t retryDelay = MQTT_RETRY_MIN_MS;
  uint32_t nextTry = 0, lastDiag = 0, lastState = 0;

  for (;;) {
    if (removeRequested) {
      if (client.connected()) removeFromHa();
      removeRequested = false;
      removeDone = true;
    }

    if (cfgGen != seenGen) {
      xSemaphoreTake(cfgLock, portMAX_DELAY);
      MqttConfig next = sharedCfg;
      seenGen = cfgGen;
      xSemaphoreGive(cfgLock);
      if (client.connected()) {
        if (!next.enabled) removeFromHa();
        else {
          // Reconnect so broker, credentials and device name changes all take effect.
          client.publish(topicAvail, "offline", true);
          client.disconnect();
        }
      }
      cfg = next;
      status = cfg.enabled ? 1 : 0;  // our own disconnect is not a dropped session
      retryDelay = MQTT_RETRY_MIN_MS;
      nextTry = millis();
    }

    if (!cfg.enabled) {
      status = 0;
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      status = 1;
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    uint32_t now = millis();
    if (!client.connected()) {
      if (status == 2) lastError = client.state();  // dropped after a working session
      status = 1;
      if ((int32_t)(now - nextTry) < 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
      if (connectBroker(cfg)) {
        retryDelay = MQTT_RETRY_MIN_MS;
        lastDiag = lastState = millis();
      } else {
        nextTry = millis() + retryDelay;
        retryDelay = min(retryDelay * 2, MQTT_RETRY_MAX_MS);
      }
      continue;
    }

    status = 2;
    client.loop();
    if (rediscover) {
      rediscover = false;
      publishDiscovery(cfg);
      client.publish(topicAvail, "online", true);
      stateDirty = true;
      lastDiag = 0;
    }
    if (stateDirty && now - lastState >= STATE_MIN_INTERVAL_MS) {
      stateDirty = false;
      lastState = now;
      publishState();
    }
    if (lastDiag == 0 || now - lastDiag >= MQTT_DIAG_INTERVAL_MS) {
      lastDiag = now ? now : 1;
      publishDiag();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void mqttBegin() {
  uint64_t mac = ESP.getEfuseMac();
  char id[13];
  snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", (uint8_t)mac, (uint8_t)(mac >> 8),
           (uint8_t)(mac >> 16), (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  snprintf(nodeId, sizeof(nodeId), "ledbar_%s", id);
  snprintf(topicState, sizeof(topicState), "ledbar/%s/state", id);
  snprintf(topicSet, sizeof(topicSet), "ledbar/%s/set", id);
  snprintf(topicAvail, sizeof(topicAvail), "ledbar/%s/status", id);
  snprintf(topicDiag, sizeof(topicDiag), "ledbar/%s/diag", id);
  snprintf(topicConfig, sizeof(topicConfig), "homeassistant/device/%s/config", nodeId);

  client.setCallback(onMessage);
  client.setBufferSize(2048);  // device discovery payload is ~1.3 KB
  client.setSocketTimeout(3);
  client.setKeepAlive(30);

  cfgLock = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(8, sizeof(Command));
  mqttConfigChanged();
  xTaskCreate(mqttTask, "mqtt", 6144, nullptr, 1, nullptr);
}

void mqttConfigChanged() {
  xSemaphoreTake(cfgLock, portMAX_DELAY);
  sharedCfg.enabled = settings.mqttEnabled;
  strlcpy(sharedCfg.host, settings.mqttHost, sizeof(sharedCfg.host));
  sharedCfg.port = settings.mqttPort;
  strlcpy(sharedCfg.user, settings.mqttUser, sizeof(sharedCfg.user));
  strlcpy(sharedCfg.pass, settings.mqttPass, sizeof(sharedCfg.pass));
  strlcpy(sharedCfg.name, settings.deviceName, sizeof(sharedCfg.name));
  cfgGen = cfgGen + 1;
  lastError = ERR_NONE;
  xSemaphoreGive(cfgLock);
}

void mqttLoop() {
  Command cmd;
  while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) {
    if (cmd.on == 0 || cmd.level == 0) led.turnOff();
    else if (cmd.level > 0) led.setLevel(cmd.level * 10);
    else if (cmd.on == 1) led.turnOn();
  }

  bool on = led.isOn();
  uint8_t pct = (led.level() + 5) / 10;
  if (on != lightOn || pct != lightPct) {
    lightOn = on;
    lightPct = pct;
    stateDirty = true;
  }
}

void mqttRemoveDevice() {
  if (status != 2) return;
  removeDone = false;
  removeRequested = true;
  for (int i = 0; i < 30 && !removeDone; i++) delay(50);
}

const char *mqttError() {
  switch (lastError) {
    case ERR_NONE: return "";
    case ERR_DNS: return "Cannot resolve the broker host name. Check the address or use an IP.";
    case MQTT_CONNECT_FAILED: return "Broker not reachable. Check the IP address and port.";
    case MQTT_CONNECTION_TIMEOUT: return "Broker did not answer in time.";
    case MQTT_CONNECTION_LOST: return "Connection to the broker was lost.";
    case MQTT_CONNECT_BAD_CREDENTIALS:
    case MQTT_CONNECT_UNAUTHORIZED: return "Login rejected. Check the username and password.";
    default: return "Broker refused the connection.";
  }
}

const char *mqttStatus() {
  static const char *names[] = {"disabled", "connecting", "connected"};
  return names[status];
}
