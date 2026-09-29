#pragma once
#include <Arduino.h>
#include "config.h"

typedef void (*TouchZoneCallback)(TouchZone zone);

void touchInit();
void touchPoll();
void touchSetCallback(TouchZoneCallback cb);
TouchZone touchHitTest(int16_t x, int16_t y);
