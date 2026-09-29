#pragma once

#include <Arduino.h>

#ifndef CFG_POLL_MS
#define CFG_POLL_MS 10UL
#endif

#ifndef CFG_WEBHOOK_BASE
#define CFG_WEBHOOK_BASE "https://example.invalid/webhook"
#endif

#ifndef CFG_MQTT_PREFIX
#define CFG_MQTT_PREFIX "grokbot/round"
#endif

#define MQTT_TOPIC_UI_HOME  CFG_MQTT_PREFIX "/ui/home"
#define MQTT_TOPIC_UI_STATE CFG_MQTT_PREFIX "/ui/state"
#define MQTT_TOPIC_STATUS   CFG_MQTT_PREFIX "/status"
#define MQTT_TOPIC_ANIM     CFG_MQTT_PREFIX "/anim"

#define LCD_H_RES 480
#define LCD_V_RES 480

enum class TouchZone : uint8_t {
  None = 0,
  Z_SLOT_A,
  Z_SLOT_B,
  Z_SLOT_C,
  Z_ACTION_PRIMARY,
  Z_ACTION_BACK,
  Z_WIFI,
};

enum class WebhookEvent : uint8_t {
  SLOT_A = 0,
  SLOT_B,
  SLOT_C,
  PRIMARY,
  BACK,
  WIFI,
};

inline const char* webhookEventPath(WebhookEvent e) {
  switch (e) {
    case WebhookEvent::SLOT_A:  return "/slot/a";
    case WebhookEvent::SLOT_B:  return "/slot/b";
    case WebhookEvent::SLOT_C:  return "/slot/c";
    case WebhookEvent::PRIMARY: return "/action/primary";
    case WebhookEvent::BACK:    return "/action/back";
    case WebhookEvent::WIFI:    return "/action/wifi";
    default:                    return "/unknown";
  }
}

inline const char* webhookEventName(WebhookEvent e) {
  switch (e) {
    case WebhookEvent::SLOT_A:  return "SLOT_A";
    case WebhookEvent::SLOT_B:  return "SLOT_B";
    case WebhookEvent::SLOT_C:  return "SLOT_C";
    case WebhookEvent::PRIMARY: return "PRIMARY";
    case WebhookEvent::BACK:    return "BACK";
    case WebhookEvent::WIFI:    return "WIFI";
    default:                    return "UNKNOWN";
  }
}

inline const char* webhookEventSlot(WebhookEvent e) {
  switch (e) {
    case WebhookEvent::SLOT_A: return "A";
    case WebhookEvent::SLOT_B: return "B";
    case WebhookEvent::SLOT_C: return "C";
    default:                   return "";
  }
}
