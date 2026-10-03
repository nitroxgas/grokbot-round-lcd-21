#pragma once

/** Configure the client from secrets.h. No-op (disabled) when host, user,
 *  password or topic is empty. */
void mqttInit();
/** True when secrets.h has host, user, password and topic. */
bool mqttEnabled();
/** Connect / subscribe / reconnect / pump. Call every loop(). */
void mqttLoop();
bool mqttConnected();
/** Apply one JSON payload from the single subscribed topic. The body kind is
 *  told apart by its fields: anim · state · slot+text · title/message.
 *  False when no known field is present. */
bool mqttApply(const char* json);
