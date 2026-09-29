#pragma once
#include <stdint.h>

enum class Locale : uint8_t {
  Pt = 0,
  En = 1,
  Es = 2,
};

enum class I18nId : uint16_t {
  TitleDefault = 0,
  MsgIdle,
  StateBoot,
  StateHome,
  StateFleet,
  StateTouchConfirm,
  StateWorking,
  StateDone,
  StateError,
  SlotA,
  SlotB,
  SlotC,
  ActionPrimary,
  ActionBack,
  Wifi,
  ActionOk,
  ActionRetry,
  ActionHome,
  Online,
  FleetLabel,
  TouchSlotHint,
  MsgTouchOrMqtt,
  TitleIdle,
  TitleWebhookSent,
  MsgActionCompleted,
  TitleDoneDefault,
  MsgDoneDefault,
  MsgTouchRetry,
  TitleErrorDefault,
  SlotNameA,
  SlotNameB,
  SlotNameC,
  SlotStatusIdle,
  SlotStatusWorking,
  SlotStatusStandby,
  Count,
};

void i18nSetLocale(Locale locale);
Locale i18nLocale();
const char* i18nStr(I18nId id);
