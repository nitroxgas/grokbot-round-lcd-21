#pragma once

void mqttInit();
/** Apply one payload. False when the topic is not handled. */
bool mqttApply(const char* topic, const char* json);
