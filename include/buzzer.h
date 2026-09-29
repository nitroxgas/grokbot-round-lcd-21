#pragma once
#include <Arduino.h>

enum class BuzzerPattern : uint8_t {
  TAP = 0,
  CONFIRM,
  ERROR,
  BOOT,
};

void buzzerInit();
void buzzerSetEnabled(bool enabled);
bool buzzerEnabled();
void buzzerPulse(BuzzerPattern pattern);
