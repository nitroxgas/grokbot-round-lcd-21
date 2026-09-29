#pragma once
#include <Arduino.h>
#include "config.h"

enum class UiState : uint8_t {
  BOOT = 0,
  HOME,
  FLEET_STATUS,
  TOUCH_CONFIRM,
  WORKING,
  DONE,
  ERROR,
};

const char* uiStateName(UiState s);

void displayInit();
void displaySetState(UiState s);
UiState displayGetState();
void displayTick();

void displaySetTitle(const char* title);
void displaySetMessage(const char* message);
void displaySetSlot(char slot, const char* text);  // slot 'A'|'B'|'C'
void displaySetAnim(const char* anim, char slotOpt); // idle|working|done|error

/** Shared zone handler for LVGL buttons + CST820 hit-test (debounce + webhook). */
void displayFireZone(TouchZone zone);
