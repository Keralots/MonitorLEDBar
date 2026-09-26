#include "network.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"
#include "settings.h"
#include "web.h"

static WiFiManager wm;
static bool portalActive = false;
static bool everConnected = false;
static bool mdnsUp = false;
static uint32_t staStartedAt = 0;
static char apName[32];

static void startPortal() {
  Serial.printf("WiFi: starting setup portal %s\n", apName);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(wm.getWiFiIsSaved() ? WIFI_PORTAL_TIMEOUT_S : 0);
  wm.startConfigPortal(apName);
  portalActive = true;
}

static void startSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();  // stored credentials
  staStartedAt = millis();
}

void networkBegin() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(apName, sizeof(apName), "LEDBar-%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  WiFi.setHostname(settings.deviceName);  // must precede the first mode() call to reach DHCP
  WiFi.mode(WIFI_STA);  // WiFi must be initialised before reading stored credentials
  WiFi.setSleep(false);  // modem sleep drops ESP-NOW frames from paired bars
  wm.setDebugOutput(false);
  wm.setConnectTimeout(20);
#ifdef PROVISION_SSID
  // Bench provisioning of a blank board; flags come from PLATFORMIO_BUILD_FLAGS, never from files.
#define PROV_STR2(x) #x
#define PROV_STR(x) PROV_STR2(x)
  if (!wm.getWiFiIsSaved()) WiFi.begin(PROV_STR(PROVISION_SSID), PROV_STR(PROVISION_PASS));
#endif
  if (wm.getWiFiIsSaved()) startSta();
  else startPortal();
}

void networkRestartMdns() {
  if (mdnsUp) MDNS.end();
  mdnsUp = MDNS.begin(settings.deviceName);
  if (mdnsUp) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "version", FW_VERSION);
    MDNS.addServiceTxt("http", "tcp", "model", "MonitorLEDBar");
  }
}

void networkLoop(uint32_t now) {
  if (portalActive) {
    if (wm.process()) {
      // Credentials saved by the portal: reboot into a clean STA-only session.
      Serial.println("WiFi: configured, restarting");
      delay(500);
      ESP.restart();
    }
    if (!wm.getConfigPortalActive()) {
      portalActive = false;
      startSta();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!everConnected) {
      everConnected = true;
      Serial.printf("WiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
      networkRestartMdns();
      webBegin();
    }
    return;
  }

  // Never connected with stored credentials (wrong password, new router): offer the portal.
  if (!everConnected && now - staStartedAt > WIFI_FALLBACK_PORTAL_MS) startPortal();
}

bool networkConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void networkResetCredentials() {
  wm.resetSettings();
}
