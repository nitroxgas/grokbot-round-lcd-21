#include "wifi_stub.h"
#include <Arduino.h>

// WiFiManager header pulled so the dependency links; portal not started in scaffold.
#include <WiFiManager.h>

namespace {
WiFiManager* g_wm = nullptr;
bool g_connected = false;
}  // namespace

void wifiStubInit() {
  if (g_wm == nullptr) {
    g_wm = new WiFiManager();
  }
  Serial.println(F("[wifi] WiFiManager placeholder (no portal auto-start)"));
  g_connected = false;
}

bool wifiStubConnected() { return g_connected; }

void wifiStubStartPortal() {
  if (g_wm == nullptr) {
    wifiStubInit();
  }
  Serial.println(F("[wifi] portal stub — real autoConnect later"));
  // Intentionally do not call autoConnect() in scaffold (blocks / needs AP).
}
