#include "mqtt.h"
#include "config.h"
#include "display.h"
#include <cstdio>
#include <cstring>

namespace {

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

}  // namespace

void mqttInit() {
  Serial.println(F("[mqtt] stub, no broker"));
  Serial.print(F("[mqtt] in "));
  Serial.println(MQTT_TOPIC_UI_HOME);
  Serial.print(F("[mqtt] in "));
  Serial.println(MQTT_TOPIC_UI_STATE);
  Serial.print(F("[mqtt] in "));
  Serial.println(MQTT_TOPIC_STATUS);
  Serial.print(F("[mqtt] in "));
  Serial.println(MQTT_TOPIC_ANIM);
}

bool mqttApply(const char* topic, const char* json) {
  if (topic == nullptr || json == nullptr) {
    return false;
  }
  if (std::strcmp(topic, MQTT_TOPIC_UI_HOME) == 0) {
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
  if (std::strcmp(topic, MQTT_TOPIC_UI_STATE) == 0) {
    char name[24];
    if (!copyJsonString(json, "state", name, sizeof(name))) {
      return false;
    }
    displaySetState(stateFromName(name));
    return true;
  }
  if (std::strcmp(topic, MQTT_TOPIC_STATUS) == 0) {
    char slot[8];
    char text[64];
    const bool hasSlot = copyJsonString(json, "slot", slot, sizeof(slot));
    const bool hasText = copyJsonString(json, "text", text, sizeof(text));
    if (hasSlot && hasText) {
      displaySetSlot(slot[0], text);
    } else if (hasText) {
      displaySetMessage(text);
    }
    displaySetState(UiState::FLEET_STATUS);
    return true;
  }
  if (std::strcmp(topic, MQTT_TOPIC_ANIM) == 0) {
    char anim[16];
    char slot[8] = {0};
    if (!copyJsonString(json, "anim", anim, sizeof(anim))) {
      return false;
    }
    copyJsonString(json, "slot", slot, sizeof(slot));
    displaySetAnim(anim, slot[0]);
    return true;
  }
  return false;
}
