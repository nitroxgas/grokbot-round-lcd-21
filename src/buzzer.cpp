#include "buzzer.h"
#include "pins.h"
#include "tca9554.h"

namespace {
bool g_enabled = true;

void toneMs(uint16_t onMs, uint16_t offMs, uint8_t reps) {
  if (!g_enabled) {
    return;
  }
  for (uint8_t i = 0; i < reps; ++i) {
    tca9554SetPin(EXIO_BUZZER, true);
    delay(onMs);
    tca9554SetPin(EXIO_BUZZER, false);
    if (offMs > 0 && i + 1 < reps) {
      delay(offMs);
    }
  }
}
}  // namespace

void buzzerInit() {
  tca9554SetDirection(EXIO_BUZZER, false);
  tca9554SetPin(EXIO_BUZZER, false);
  Serial.println(F("[buzzer] EXIO8 ready"));
}

void buzzerSetEnabled(bool enabled) { g_enabled = enabled; }
bool buzzerEnabled() { return g_enabled; }

void buzzerPulse(BuzzerPattern pattern) {
  switch (pattern) {
    case BuzzerPattern::TAP:
      toneMs(15, 0, 1);
      break;
    case BuzzerPattern::CONFIRM:
      toneMs(40, 30, 2);
      break;
    case BuzzerPattern::ERROR:
      toneMs(80, 40, 3);
      break;
    case BuzzerPattern::BOOT:
      toneMs(25, 0, 1);
      break;
  }
}
