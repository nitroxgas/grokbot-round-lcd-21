#include "mqtt.h"
#include "config.h"
#include "display.h"
#include "wifi_stub.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kReconnectMs = 5000;
constexpr size_t kPayloadMax = 511;

WiFiClient g_plain;
WiFiClientSecure g_tls;
PubSubClient g_client;
char g_clientId[32] = {0};
uint32_t g_lastAttemptMs = 0;
bool g_wasConnected = false;

bool hasKey(const char* json, const char* key) {
  char pattern[24];
  if (std::snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= static_cast<int>(sizeof(pattern))) {
    return false;
  }
  return std::strstr(json, pattern) != nullptr;
}

bool copyJsonString(const char* json, const char* key, char* out, size_t outLen) {
  if (json == nullptr || key == nullptr || out == nullptr || outLen == 0) {
    return false;
  }
  char pattern[24];
  if (std::snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= static_cast<int>(sizeof(pattern))) {
    return false;
  }
  const char* p = std::strstr(json, pattern);
  if (p == nullptr) {
    return false;
  }
  p += std::strlen(pattern);
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
    ++p;
  }
  if (*p != ':') {
    return false;
  }
  ++p;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
    ++p;
  }
  if (*p != '"') {
    return false;
  }
  ++p;
  size_t n = 0;
  while (*p != '\0' && *p != '"' && n + 1 < outLen) {
    if (*p == '\\' && p[1] != '\0') {
      ++p;
    }
    out[n++] = *p++;
  }
  out[n] = '\0';
  return *p == '"';
}

UiState stateFromName(const char* name) {
  if (std::strcmp(name, "BOOT") == 0) return UiState::BOOT;
  if (std::strcmp(name, "HOME") == 0) return UiState::HOME;
  if (std::strcmp(name, "FLEET_STATUS") == 0) return UiState::FLEET_STATUS;
  if (std::strcmp(name, "TOUCH_CONFIRM") == 0) return UiState::TOUCH_CONFIRM;
  if (std::strcmp(name, "WORKING") == 0) return UiState::WORKING;
  if (std::strcmp(name, "DONE") == 0) return UiState::DONE;
  if (std::strcmp(name, "ERROR") == 0) return UiState::ERROR;
  return UiState::ERROR;
}

void onMessage(char* topic, uint8_t* payload, unsigned int length) {
  (void)topic;  // single subscription: only MQTT_TOPIC ever arrives here
  static char buf[kPayloadMax + 1];
  const size_t n = length > kPayloadMax ? kPayloadMax : length;
  std::memcpy(buf, payload, n);
  buf[n] = '\0';
  if (!mqttApply(buf)) {
    Serial.print(F("[mqtt] unhandled payload: "));
    Serial.println(buf);
  }
}

bool connectAndSubscribe() {
  Serial.print(F("[mqtt] connecting "));
  Serial.print(MQTT_HOST);
  Serial.print(':');
  Serial.print(MQTT_PORT);
  Serial.println(MQTT_TLS ? F(" (tls)") : F(" (plain)"));
  if (!g_client.connect(g_clientId, MQTT_USER, MQTT_PASS)) {
    Serial.print(F("[mqtt] connect failed rc="));
    Serial.println(g_client.state());
    return false;
  }
  // One subscription: the person's topic from secrets.h.
  if (!g_client.subscribe(MQTT_TOPIC)) {
    Serial.println(F("[mqtt] subscribe failed"));
    g_client.disconnect();
    return false;
  }
  Serial.print(F("[mqtt] subscribed (secrets.h) "));
  Serial.println(MQTT_TOPIC);
  return true;
}

}  // namespace

bool mqttEnabled() {
  return MQTT_HOST[0] != '\0' && MQTT_USER[0] != '\0' && MQTT_PASS[0] != '\0' &&
         MQTT_TOPIC[0] != '\0';
}

bool mqttConnected() { return mqttEnabled() && g_client.connected(); }

void mqttInit() {
  if (!mqttEnabled()) {
    Serial.println(F("[mqtt] disabled: host/user/pass/topic empty in secrets.h (no connect)"));
    return;
  }
  if (MQTT_TLS) {
    // No CA embedded: TLS is encryption-only (see docs/mqtt-webhook.md).
    g_tls.setInsecure();
    g_client.setClient(g_tls);
  } else {
    g_client.setClient(g_plain);
  }
  g_client.setServer(MQTT_HOST, MQTT_PORT);
  g_client.setCallback(onMessage);
  g_client.setBufferSize(kPayloadMax + 1 + 128);
  g_client.setKeepAlive(30);
  g_client.setSocketTimeout(5);
  std::snprintf(g_clientId, sizeof(g_clientId), "round-lcd-21-%06X",
                static_cast<unsigned>(ESP.getEfuseMac() & 0xFFFFFFu));
  Serial.print(F("[mqtt] client "));
  Serial.print(g_clientId);
  Serial.print(F(" · topic "));
  Serial.println(MQTT_TOPIC);
}

void mqttLoop() {
  if (!mqttEnabled()) {
    return;
  }
  if (!wifiStubConnected()) {
    g_wasConnected = false;
    return;
  }
  if (!g_client.connected()) {
    if (g_wasConnected) {
      Serial.println(F("[mqtt] connection lost, will reconnect"));
      g_wasConnected = false;
    }
    const uint32_t now = millis();
    if (g_lastAttemptMs != 0 && (now - g_lastAttemptMs) < kReconnectMs) {
      return;
    }
    g_lastAttemptMs = now;
    g_wasConnected = connectAndSubscribe();
    return;
  }
  g_client.loop();
}

bool mqttApply(const char* json) {
  if (json == nullptr) {
    return false;
  }
  // Same four bodies as before, now on one topic; "anim" and "state" are
  // checked first because "slot" also appears in the status body.
  if (hasKey(json, "anim")) {
    char anim[16];
    char slot[8] = {0};
    if (!copyJsonString(json, "anim", anim, sizeof(anim))) {
      return false;
    }
    copyJsonString(json, "slot", slot, sizeof(slot));
    displaySetAnim(anim, slot[0]);
    return true;
  }
  if (hasKey(json, "state")) {
    char name[24];
    if (!copyJsonString(json, "state", name, sizeof(name))) {
      return false;
    }
    displaySetState(stateFromName(name));
    return true;
  }
  if (hasKey(json, "text")) {
    char slot[8];
    char text[64];
    const bool hasSlot = copyJsonString(json, "slot", slot, sizeof(slot));
    const bool hasText = copyJsonString(json, "text", text, sizeof(text));
    if (!hasText) {
      return false;
    }
    if (hasSlot) {
      displaySetSlot(slot[0], text);
    } else {
      displaySetMessage(text);
    }
    displaySetState(UiState::FLEET_STATUS);
    return true;
  }
  if (hasKey(json, "title") || hasKey(json, "message")) {
    char title[40];
    char message[80];
    if (copyJsonString(json, "title", title, sizeof(title))) {
      displaySetTitle(title);
    }
    if (copyJsonString(json, "message", message, sizeof(message))) {
      displaySetMessage(message);
    }
    displaySetState(UiState::HOME);
    return true;
  }
  return false;
}
