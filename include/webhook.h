#pragma once
#include <Arduino.h>
#include "config.h"

void webhookInit();
void webhookFire(WebhookEvent event);
bool webhookPostStub(const char* url, const char* jsonBody);
