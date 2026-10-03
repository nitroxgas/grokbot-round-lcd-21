#pragma once

#include <Arduino.h>

#ifndef CFG_POLL_MS
#define CFG_POLL_MS 10UL
#endif

// Webhook base/token and MQTT broker/topic live only in include/secrets.h
// (gitignored, compiled into each person's binary). Without that file every
// field is empty, which disables the webhook POST and the MQTT connection.
// There is intentionally no baked-in URL or topic here.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h missing: webhook + MQTT disabled (cp include/secrets.h.example include/secrets.h)"
#endif

#ifndef WEBHOOK_BASE
#define WEBHOOK_BASE ""
#endif
#ifndef WEBHOOK_TOKEN
#define WEBHOOK_TOKEN ""
#endif
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif
#ifndef MQTT_TLS
#define MQTT_TLS 1
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif
#ifndef MQTT_TOPIC
#define MQTT_TOPIC ""
#endif

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
