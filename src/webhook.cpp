#include "webhook.h"
#include <cstdio>

void webhookInit() {
  Serial.print(F("[webhook] stub base="));
  Serial.println(CFG_WEBHOOK_BASE);
}

void webhookFire(WebhookEvent event) {
  char url[192];
  snprintf(url, sizeof(url), "%s%s", CFG_WEBHOOK_BASE, webhookEventPath(event));

  char body[128];
  const char* slot = webhookEventSlot(event);
  if (slot[0] != '\0') {
    snprintf(body, sizeof(body),
             "{\"source\":\"round-lcd-21\",\"event\":\"%s\",\"slot\":\"%s\"}",
             webhookEventName(event), slot);
  } else {
    snprintf(body, sizeof(body),
             "{\"source\":\"round-lcd-21\",\"event\":\"%s\",\"slot\":\"\"}",
             webhookEventName(event));
  }

  Serial.print(F("[webhook] FIRE "));
  Serial.println(url);
  webhookPostStub(url, body);
}

bool webhookPostStub(const char* url, const char* jsonBody) {
  Serial.print(F("[webhook] POST stub url="));
  Serial.print(url);
  Serial.print(F(" body="));
  Serial.println(jsonBody);
  return true;
}
