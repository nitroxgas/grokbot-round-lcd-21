#pragma once
#include <Arduino.h>

bool tca9554Init();
bool tca9554SetPin(uint8_t exio, bool high);
bool tca9554GetPin(uint8_t exio, bool* highOut);
bool tca9554SetDirection(uint8_t exio, bool input);
