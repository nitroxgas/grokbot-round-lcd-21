#include "webhook.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint16_t kTimeoutMs = 3000;
}  // namespace

bool webhookEnabled() { return WEBHOOK_BASE[0] != '\0'; }

void webhookInit() {
  if (!webhookEnabled()) {
    Serial.println(F("[webhook] disabled: WEBHOOK_BASE empty in secrets.h (no POST)"));
    return;
  }
  Serial.print(F("[webhook] base (secrets.h)="));
  Serial.print(WEBHOOK_BASE);
  Serial.println(WEBHOOK_TOKEN[0] != '\0' ? F(" · bearer token set") : F(" · no token"));
}

void webhookFire(WebhookEvent event) {
  char body[128];
  snprintf(body, sizeof(body),
           "{\"source\":\"round-lcd-21\",\"event\":\"%s\",\"slot\":\"%s\"}",
           webhookEventName(event), webhookEventSlot(event));

  if (!webhookEnabled()) {
    Serial.print(F("[webhook] skip (no base) "));
    Serial.println(body);
    return;
  }

  char url[256];
  snprintf(url, sizeof(url), "%s%s", WEBHOOK_BASE, webhookEventPath(event));

  Serial.print(F("[webhook] FIRE "));
  Serial.println(url);
  webhookPost(url, body);
}

bool webhookPost(const char* url, const char* jsonBody) {
  if (url == nullptr || jsonBody == nullptr || url[0] == '\0') {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[webhook] POST skipped: WiFi not connected"));
    return false;
  }

  const bool https = std::strncmp(url, "https://", 8) == 0;
  WiFiClient plain;
  WiFiClientSecure tls;
  if (https) {
    // No CA embedded: TLS is encryption-only (see docs/mqtt-webhook.md).
    tls.setInsecure();
  }
  WiFiClient& client = https ? static_cast<WiFiClient&>(tls) : plain;

  HTTPClient http;
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    Serial.println(F("[webhook] POST begin() failed (bad url?)"));
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  if (WEBHOOK_TOKEN[0] != '\0') {
    http.addHeader("Authorization", String("Bearer ") + WEBHOOK_TOKEN);
  }

  const int code = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(jsonBody)),
                             std::strlen(jsonBody));
  http.end();

  Serial.print(F("[webhook] POST "));
  Serial.print(url);
  Serial.print(F(" -> "));
  if (code < 0) {
    Serial.println(HTTPClient::errorToString(code));
    return false;
  }
  Serial.println(code);
  return code >= 200 && code < 300;
}
