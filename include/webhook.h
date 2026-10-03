#pragma once
#include <Arduino.h>
#include "config.h"

void webhookInit();
/** True when WEBHOOK_BASE (secrets.h) is non-empty. */
bool webhookEnabled();
void webhookFire(WebhookEvent event);
/** Real HTTP(S) POST of jsonBody to url. False when disabled, offline or non-2xx. */
bool webhookPost(const char* url, const char* jsonBody);
