#pragma once
#include <Arduino.h>

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
