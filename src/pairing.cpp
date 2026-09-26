#include "pairing.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "app.h"
#include "config.h"
#include "settings.h"

namespace {

constexpr uint16_t MAGIC = 0x4C42;  // "LB"
constexpr uint8_t PROTO_VERSION = 1;
constexpr uint8_t MAX_PEERS = 7;  // group of 8 bars
constexpr uint8_t MAX_NEARBY = 12;
constexpr uint32_t HELLO_INTERVAL_MS = 3000;
constexpr uint32_t SEEN_TIMEOUT_MS = 10000;
constexpr uint32_t STATE_MIN_INTERVAL_MS = 40;
const uint8_t BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

enum MsgType : uint8_t { HELLO = 1, JOIN = 2, MEMBERS = 3, STATE = 4, SYNC_REQ = 5 };

struct __attribute__((packed)) Header {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint32_t groupId;
};
struct __attribute__((packed)) HelloMsg {
  Header h;
  char name[32];
};
struct __attribute__((packed)) MembersMsg {
  Header h;
  uint8_t count;
  uint8_t macs[MAX_PEERS + 1][6];
};
struct __attribute__((packed)) StateMsg {
  Header h;
  uint8_t on;
  uint16_t level;
};

struct Peer {
  uint8_t mac[6];
  char name[32];
  uint32_t lastSeen;
};
struct Nearby {
  uint8_t mac[6];
  char name[32];
  uint32_t groupId;
  uint32_t lastSeen;
};
struct Rx {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[ESP_NOW_MAX_DATA_LEN];
};

uint8_t selfMac[6];
uint32_t groupId = 0;
Peer peers[MAX_PEERS];
uint8_t peerCount = 0;
Nearby nearby[MAX_NEARBY];
uint8_t nearbyCount = 0;
QueueHandle_t rxQueue;
bool ready = false;
bool syncRequested = false;
// Last state known to be shared with the group; local changes differ from it, remote ones do not.
bool groupOn = false;
uint16_t groupLevel = 0;
uint32_t lastHello = 0, lastStateSent = 0;
Preferences prefs;

bool macEq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

String macStr(const uint8_t *m) {
  char s[18];
  snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(s);
}

int findPeer(const uint8_t *mac) {
  for (int i = 0; i < peerCount; i++)
    if (macEq(peers[i].mac, mac)) return i;
  return -1;
}

Nearby *findNearby(const uint8_t *mac) {
  for (int i = 0; i < nearbyCount; i++)
    if (macEq(nearby[i].mac, mac)) return &nearby[i];
  return nullptr;
}

void saveGroup() {
  prefs.begin("pair", false);
  prefs.putUInt("gid", groupId);
  prefs.putUChar("count", peerCount);
  for (int i = 0; i < peerCount; i++) {
    char key[4] = {'p', (char)('0' + i), 0};
    prefs.putBytes(key, &peers[i], offsetof(Peer, lastSeen));  // mac + name
  }
  prefs.end();
}

void loadGroup() {
  prefs.begin("pair", true);
  groupId = prefs.getUInt("gid", 0);
  peerCount = min<uint8_t>(prefs.getUChar("count", 0), MAX_PEERS);
  for (int i = 0; i < peerCount; i++) {
    char key[4] = {'p', (char)('0' + i), 0};
    memset(&peers[i], 0, sizeof(Peer));
    prefs.getBytes(key, &peers[i], offsetof(Peer, lastSeen));
    peers[i].name[sizeof(peers[i].name) - 1] = 0;
  }
  prefs.end();
  if (peerCount == 0) groupId = 0;
}

void ensureEspNowPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t info = {};
  memcpy(info.peer_addr, mac, 6);
  info.channel = 0;  // follow the current WiFi channel
  info.ifidx = WIFI_IF_STA;
  info.encrypt = false;
  if (esp_now_add_peer(&info) != ESP_OK) Serial.printf("Pair: cannot add peer %s\n", macStr(mac).c_str());
}

void dropEspNowPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
}

void fillHeader(Header &h, MsgType type) {
  h.magic = MAGIC;
  h.version = PROTO_VERSION;
  h.type = type;
  h.groupId = groupId;
}

void sendTo(const uint8_t *mac, const void *data, size_t len) {
  ensureEspNowPeer(mac);
  esp_now_send(mac, (const uint8_t *)data, len);
}

void sendHello() {
  HelloMsg m = {};
  fillHeader(m.h, HELLO);
  strlcpy(m.name, settings.deviceName, sizeof(m.name));
  sendTo(BROADCAST, &m, sizeof(m));
}

// Full member list including this bar.
void buildMembers(MembersMsg &m, MsgType type) {
  memset(&m, 0, sizeof(m));
  fillHeader(m.h, type);
  memcpy(m.macs[m.count++], selfMac, 6);
  for (int i = 0; i < peerCount; i++) memcpy(m.macs[m.count++], peers[i].mac, 6);
}

void sendMembersToAll(const MembersMsg &m) {
  for (int i = 0; i < peerCount; i++) sendTo(peers[i].mac, &m, sizeof(m));
}

void sendState(const uint8_t *mac) {
  StateMsg m = {};
  fillHeader(m.h, STATE);
  m.on = led.isOn();
  m.level = led.level();
  sendTo(mac, &m, sizeof(m));
}

void markShared() {
  groupOn = led.isOn();
  groupLevel = led.level();
}

void clearGroup() {
  for (int i = 0; i < peerCount; i++) dropEspNowPeer(peers[i].mac);
  peerCount = 0;
  groupId = 0;
  saveGroup();
  Serial.println("Pair: left group");
}

// Replace the member list from a MEMBERS/JOIN message; names come from the old list or HELLOs.
void adoptMembers(const MembersMsg &m, uint32_t newGroup) {
  Peer next[MAX_PEERS];
  uint8_t n = 0;
  bool selfListed = false;
  for (int i = 0; i < m.count && i <= MAX_PEERS; i++) {
    const uint8_t *mac = m.macs[i];
    if (macEq(mac, selfMac)) {
      selfListed = true;
      continue;
    }
    if (n >= MAX_PEERS) break;
    Peer &p = next[n++];
    memset(&p, 0, sizeof(p));
    memcpy(p.mac, mac, 6);
    int old = findPeer(mac);
    Nearby *nb = findNearby(mac);
    if (old >= 0) {
      strlcpy(p.name, peers[old].name, sizeof(p.name));
      p.lastSeen = peers[old].lastSeen;
    } else if (nb) {
      strlcpy(p.name, nb->name, sizeof(p.name));
      p.lastSeen = nb->lastSeen;
    }
  }
  if (!selfListed || n == 0) {
    clearGroup();
    return;
  }
  for (int i = 0; i < peerCount; i++) {
    bool kept = false;
    for (int j = 0; j < n; j++) kept |= macEq(peers[i].mac, next[j].mac);
    if (!kept) dropEspNowPeer(peers[i].mac);
  }
  memcpy(peers, next, sizeof(Peer) * n);
  peerCount = n;
  groupId = newGroup;
  saveGroup();
  Serial.printf("Pair: group %08lx, %u peers\n", (unsigned long)groupId, peerCount);
}

void handleHello(const uint8_t *mac, const HelloMsg &m) {
  char name[32];
  strlcpy(name, m.name, sizeof(name));
  Nearby *nb = findNearby(mac);
  if (!nb) {
    if (nearbyCount < MAX_NEARBY) nb = &nearby[nearbyCount++];
    else {
      nb = &nearby[0];  // table full: reuse the stalest entry
      for (int i = 1; i < nearbyCount; i++)
        if (nearby[i].lastSeen < nb->lastSeen) nb = &nearby[i];
    }
    memcpy(nb->mac, mac, 6);
  }
  strlcpy(nb->name, name, sizeof(nb->name));
  nb->groupId = m.h.groupId;
  nb->lastSeen = millis();

  int i = findPeer(mac);
  if (i >= 0) {
    peers[i].lastSeen = millis();
    if (strcmp(peers[i].name, name) != 0) {
      strlcpy(peers[i].name, name, sizeof(peers[i].name));
      saveGroup();
    }
  }
}

void handleMessage(const Rx &rx) {
  const Header &h = *(const Header *)rx.data;
  if (h.magic != MAGIC || h.version != PROTO_VERSION) return;
  int peer = findPeer(rx.mac);
  if (peer >= 0) peers[peer].lastSeen = millis();

  switch (h.type) {
    case HELLO:
      if (rx.len >= sizeof(HelloMsg)) handleHello(rx.mac, *(const HelloMsg *)rx.data);
      break;

    case JOIN: {
      // Explicit pairing request aimed at this bar: leave any old group, then join.
      if (rx.len < sizeof(MembersMsg)) break;
      if (groupId && h.groupId != groupId) {
        MembersMsg bye;
        buildMembers(bye, MEMBERS);
        bye.count = 0;
        for (int i = 0; i < peerCount; i++) memcpy(bye.macs[bye.count++], peers[i].mac, 6);
        sendMembersToAll(bye);
      }
      adoptMembers(*(const MembersMsg *)rx.data, h.groupId);
      break;
    }

    case MEMBERS:
      if (rx.len < sizeof(MembersMsg) || !groupId || h.groupId != groupId || peer < 0) break;
      adoptMembers(*(const MembersMsg *)rx.data, groupId);
      break;

    case STATE: {
      if (rx.len < sizeof(StateMsg) || !groupId || h.groupId != groupId) break;
      if (peer < 0) {
        // A removed bar that missed the news: tell it the current member list.
        MembersMsg m;
        buildMembers(m, MEMBERS);
        sendTo(rx.mac, &m, sizeof(m));
        break;
      }
      const StateMsg &s = *(const StateMsg *)rx.data;
      led.setState(s.on, s.level);
      markShared();
      break;
    }

    case SYNC_REQ:
      if (groupId && h.groupId == groupId && peer >= 0) sendState(rx.mac);
      break;
  }
}

void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(Header) || len > ESP_NOW_MAX_DATA_LEN) return;
  Rx rx;
  memcpy(rx.mac, mac, 6);
  rx.len = len;
  memcpy(rx.data, data, len);
  xQueueSend(rxQueue, &rx, 0);  // handled in the main loop, not the WiFi task
}

}  // namespace

bool parseMac(const char *s, uint8_t mac[6]) {
  unsigned v[6];
  if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) mac[i] = v[i];
  return true;
}

void pairBegin() {
  loadGroup();
  markShared();
  rxQueue = xQueueCreate(16, sizeof(Rx));
  esp_wifi_get_mac(WIFI_IF_STA, selfMac);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Pair: ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  ensureEspNowPeer(BROADCAST);
  for (int i = 0; i < peerCount; i++) ensureEspNowPeer(peers[i].mac);
  ready = true;
  Serial.printf("Pair: ready, self %s, group %08lx, %u peers\n", macStr(selfMac).c_str(),
                (unsigned long)groupId, peerCount);
}

void pairLoop(uint32_t now) {
  if (!ready) return;
  Rx rx;
  while (xQueueReceive(rxQueue, &rx, 0) == pdTRUE) handleMessage(rx);

  if (now - lastHello >= HELLO_INTERVAL_MS) {
    lastHello = now;
    sendHello();
  }

  // After boot, ask the group for its current state once the radio is on the AP channel.
  if (!syncRequested && WiFi.status() == WL_CONNECTED) {
    syncRequested = true;
    if (groupId) {
      Header h;
      fillHeader(h, SYNC_REQ);
      for (int i = 0; i < peerCount; i++) sendTo(peers[i].mac, &h, sizeof(h));
    }
  }

  if (led.isOn() == groupOn && led.level() == groupLevel) return;
  if (!groupId) {
    markShared();
    return;
  }
  if (now - lastStateSent < STATE_MIN_INTERVAL_MS) return;
  lastStateSent = now;
  markShared();
  for (int i = 0; i < peerCount; i++) sendState(peers[i].mac);
}

bool pairAdd(const uint8_t mac[6]) {
  if (!ready || macEq(mac, selfMac) || findPeer(mac) >= 0 || peerCount >= MAX_PEERS) return false;
  if (!groupId) groupId = esp_random() | 1;  // never 0

  Peer &p = peers[peerCount++];
  memset(&p, 0, sizeof(p));
  memcpy(p.mac, mac, 6);
  Nearby *nb = findNearby(mac);
  if (nb) {
    strlcpy(p.name, nb->name, sizeof(p.name));
    p.lastSeen = nb->lastSeen;
  }
  saveGroup();

  MembersMsg m;
  buildMembers(m, JOIN);
  sendTo(mac, &m, sizeof(m));
  m.h.type = MEMBERS;
  for (int i = 0; i < peerCount - 1; i++) sendTo(peers[i].mac, &m, sizeof(m));
  sendState(mac);  // the new member takes this bar's current state
  Serial.printf("Pair: added %s\n", macStr(mac).c_str());
  return true;
}

bool pairRemove(const uint8_t mac[6]) {
  int i = findPeer(mac);
  if (!ready || i < 0) return false;
  uint8_t removed[6];
  memcpy(removed, mac, 6);
  for (int j = i; j < peerCount - 1; j++) peers[j] = peers[j + 1];
  peerCount--;

  MembersMsg m;
  buildMembers(m, MEMBERS);
  sendMembersToAll(m);
  sendTo(removed, &m, sizeof(m));  // it sees itself missing and leaves
  dropEspNowPeer(removed);
  if (peerCount == 0) groupId = 0;
  saveGroup();
  Serial.printf("Pair: removed %s\n", macStr(removed).c_str());
  return true;
}

void pairLeave() {
  if (!ready || !groupId) return;
  MembersMsg m;
  buildMembers(m, MEMBERS);
  // Same list without this bar.
  m.count = 0;
  for (int i = 0; i < peerCount; i++) memcpy(m.macs[m.count++], peers[i].mac, 6);
  sendMembersToAll(m);
  clearGroup();
}

void pairFillJson(JsonDocument &doc) {
  uint32_t now = millis();
  doc["ready"] = ready;
  doc["self"] = macStr(selfMac);
  doc["name"] = settings.deviceName;
  doc["grouped"] = groupId != 0;
  JsonArray members = doc["members"].to<JsonArray>();
  for (int i = 0; i < peerCount; i++) {
    JsonObject o = members.add<JsonObject>();
    o["mac"] = macStr(peers[i].mac);
    o["name"] = peers[i].name[0] ? peers[i].name : "unknown";
    o["online"] = peers[i].lastSeen && now - peers[i].lastSeen < SEEN_TIMEOUT_MS;
  }
  JsonArray near = doc["nearby"].to<JsonArray>();
  for (int i = 0; i < nearbyCount; i++) {
    if (now - nearby[i].lastSeen >= SEEN_TIMEOUT_MS || findPeer(nearby[i].mac) >= 0) continue;
    JsonObject o = near.add<JsonObject>();
    o["mac"] = macStr(nearby[i].mac);
    o["name"] = nearby[i].name;
    o["grouped"] = nearby[i].groupId != 0;
  }
}
