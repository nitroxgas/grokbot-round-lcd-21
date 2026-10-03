#include "wifi_stub.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace {
constexpr const char* kApName = "GrokBot-Round";
constexpr uint16_t kConnectTimeoutS = 15;
constexpr uint16_t kPortalTimeoutS = 300;

WiFiManager* g_wm = nullptr;
bool g_wasConnected = false;

WiFiManager& wm() {
  if (g_wm == nullptr) {
    g_wm = new WiFiManager();
    // Non-blocking so LVGL keeps ticking while the portal is up.
    g_wm->setConfigPortalBlocking(false);
    g_wm->setConnectTimeout(kConnectTimeoutS);
    g_wm->setConfigPortalTimeout(kPortalTimeoutS);
    g_wm->setDebugOutput(false);
    WiFi.setHostname("grokbot-round");
  }
  return *g_wm;
}
}  // namespace

void wifiStubInit() {
  // Tries the NVS credentials; with none saved (first boot) or on failure the
  // captive portal starts and autoConnect returns immediately.
  if (wm().autoConnect(kApName)) {
    g_wasConnected = true;
    Serial.print(F("[wifi] connected ip="));
    Serial.println(WiFi.localIP());
  } else {
    Serial.print(F("[wifi] portal started: AP "));
    Serial.println(kApName);
  }
}

void wifiStubLoop() {
  wm().process();
  const bool now = wifiStubConnected();
  if (now != g_wasConnected) {
    g_wasConnected = now;
    if (now) {
      Serial.print(F("[wifi] connected ip="));
      Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("[wifi] disconnected"));
    }
  }
}

bool wifiStubConnected() { return WiFi.status() == WL_CONNECTED; }

void wifiStubStartPortal() {
  if (wm().getConfigPortalActive()) {
    return;
  }
  Serial.print(F("[wifi] portal reopened: AP "));
  Serial.println(kApName);
  wm().startConfigPortal(kApName);
}
