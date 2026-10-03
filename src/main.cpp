#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "tca9554.h"
#include "buzzer.h"
#include "display.h"
#include "touch.h"
#include "mqtt.h"
#include "webhook.h"
#include "i18n.h"
#include "wifi_stub.h"

static uint32_t g_lastPollMs = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== grokbot-round-lcd-21 v0.3 ==="));
  Serial.println(F("SKU 30697 2.1B · Arduino-PIO+LVGL · no HA/Awtrix"));

  i18nSetLocale(Locale::Pt);
  tca9554Init();
  buzzerInit();
  wifiStubInit();
  displayInit();
  touchInit();
  webhookInit();
  mqttInit();

  buzzerPulse(BuzzerPattern::BOOT);
  displaySetState(UiState::HOME);
  Serial.println(F("[main] setup done"));
}

void loop() {
  const uint32_t now = millis();
  if ((now - g_lastPollMs) < CFG_POLL_MS) {
    return;
  }
  g_lastPollMs = now;

  displayTick();
  touchPoll();
  wifiStubLoop();
  mqttLoop();
}
