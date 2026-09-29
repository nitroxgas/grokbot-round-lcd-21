#include "display.h"
#include "pins.h"
#include "config.h"
#include "tca9554.h"
#include "i18n.h"
#include "webhook.h"
#include "buzzer.h"

#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <esp_heap_caps.h>

namespace {

// --- palette (mocks v0.2) ---
constexpr uint32_t COL_BG     = 0x0C0C0E;
constexpr uint32_t COL_TEXT   = 0xF5F5F7;
constexpr uint32_t COL_MUTED  = 0xA7A7AE;
constexpr uint32_t COL_DIM    = 0x686870;
constexpr uint32_t COL_RED    = 0xE11D2E;
constexpr uint32_t COL_GREEN  = 0x38D996;
constexpr uint32_t COL_AMBER  = 0xE8A838;
constexpr uint32_t COL_PANEL  = 0x17171B;
constexpr uint32_t COL_LINE   = 0x2A2A30;
constexpr uint32_t COL_IDLE   = 0x3A7BD5;

constexpr int16_t kCx = 240;
constexpr int16_t kCy = 240;
constexpr int16_t kDiskR = 88;       // Ø≈176
constexpr int16_t kDiskCy = 220;     // center of disk
constexpr int16_t kArcR = 165;
constexpr float kPi = 3.14159265f;

Arduino_DataBus* g_bus = nullptr;
Arduino_ESP32RGBPanel* g_rgbpanel = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;

UiState g_state = UiState::BOOT;
UiState g_lastLogged = static_cast<UiState>(0xFF);

char g_title[40] = "Grok Bot";
char g_message[80] = "idle";
char g_slotA[32] = "Idle";
char g_slotB[32] = "Working";
char g_slotC[32] = "Stand by";
char g_slotNameA[24] = "PrintMaker";
char g_slotNameB[24] = "Fab CAD";
char g_slotNameC[24] = "EspForge";

lv_disp_draw_buf_t g_drawBuf;
lv_disp_drv_t g_dispDrv;
lv_color_t* g_colorBuf = nullptr;
constexpr int kBufLines = 40;

// Shared / per-state widgets
lv_obj_t* g_screen = nullptr;
lv_obj_t* g_headerLabel = nullptr;
lv_obj_t* g_titleLabel = nullptr;
lv_obj_t* g_messageLabel = nullptr;

lv_obj_t* g_disk = nullptr;          // center anim ring
lv_obj_t* g_markCont = nullptr;      // grok mark container
lv_obj_t* g_markBody = nullptr;
lv_obj_t* g_markBar1 = nullptr;
lv_obj_t* g_markBar2 = nullptr;
lv_obj_t* g_iconCircle = nullptr;    // solid circle for check / !
lv_obj_t* g_iconLabel = nullptr;
lv_obj_t* g_workArcs[3] = {nullptr, nullptr, nullptr};
lv_obj_t* g_fleetHint = nullptr;
lv_obj_t* g_onlinePill = nullptr;
lv_obj_t* g_errorRim = nullptr;

lv_obj_t* g_contHome = nullptr;
lv_obj_t* g_contFleet = nullptr;
lv_obj_t* g_contConfirm = nullptr;
lv_obj_t* g_contWorking = nullptr;
lv_obj_t* g_contDone = nullptr;
lv_obj_t* g_contError = nullptr;
lv_obj_t* g_contBoot = nullptr;

lv_obj_t* g_fleetCard[3] = {nullptr, nullptr, nullptr};
lv_obj_t* g_fleetName[3] = {nullptr, nullptr, nullptr};
lv_obj_t* g_fleetStatus[3] = {nullptr, nullptr, nullptr};
lv_obj_t* g_fleetDot[3] = {nullptr, nullptr, nullptr};

lv_anim_t g_diskAnim;
bool g_animRunning = false;
uint32_t g_lastZoneUiMs = 0;

void panelHwReset() {
  tca9554SetDirection(EXIO_LCD_RST, false);
  tca9554SetDirection(EXIO_LCD_CS, false);
  tca9554SetPin(EXIO_LCD_CS, true);
  tca9554SetPin(EXIO_LCD_RST, false);
  delay(20);
  tca9554SetPin(EXIO_LCD_RST, true);
  delay(50);
  tca9554SetPin(EXIO_LCD_CS, false);
}

void backlightOn() {
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
}

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color) {
  if (g_gfx != nullptr) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    g_gfx->draw16bitRGBBitmap(area->x1, area->y1,
                              reinterpret_cast<uint16_t*>(&color->full), w, h);
  }
  lv_disp_flush_ready(drv);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color,
                    lv_coord_t width) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  if (width > 0) {
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  }
  return label;
}

void fireZone(TouchZone zone) {
  const uint32_t now = millis();
  if ((now - g_lastZoneUiMs) < 500) {
    return;
  }
  g_lastZoneUiMs = now;

  WebhookEvent ev = WebhookEvent::PRIMARY;
  switch (zone) {
    case TouchZone::Z_SLOT_A: ev = WebhookEvent::SLOT_A; break;
    case TouchZone::Z_SLOT_B: ev = WebhookEvent::SLOT_B; break;
    case TouchZone::Z_SLOT_C: ev = WebhookEvent::SLOT_C; break;
    case TouchZone::Z_ACTION_PRIMARY: ev = WebhookEvent::PRIMARY; break;
    case TouchZone::Z_ACTION_BACK: ev = WebhookEvent::BACK; break;
    case TouchZone::Z_WIFI: ev = WebhookEvent::WIFI; break;
    default: return;
  }
  buzzerPulse(BuzzerPattern::TAP);
  if (zone == TouchZone::Z_ACTION_BACK && g_state == UiState::DONE) {
    displaySetState(UiState::HOME);
  } else if (zone != TouchZone::Z_WIFI) {
    displaySetState(UiState::TOUCH_CONFIRM);
  }
  webhookFire(ev);
  buzzerPulse(BuzzerPattern::CONFIRM);
}

void onBtnClicked(lv_event_t* e) {
  const TouchZone z = static_cast<TouchZone>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  fireZone(z);
}

lv_obj_t* makePill(lv_obj_t* parent, const char* text, bool accent,
                   TouchZone zone, int16_t cx, int16_t cy) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 78, 36);
  lv_obj_set_pos(btn, cx - 39, cy - 18);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(accent ? COL_RED : COL_PANEL), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, accent ? 0 : 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(accent ? COL_RED : COL_LINE), 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, onBtnClicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(zone)));

  lv_obj_t* lab = lv_label_create(btn);
  lv_label_set_text(lab, text);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(COL_TEXT), 0);
  lv_obj_center(lab);
  return btn;
}

void arcPos(float angleDeg, int16_t* ox, int16_t* oy, float rScale = 0.92f,
            int16_t yOff = 10) {
  const float rad = angleDeg * kPi / 180.0f;
  *ox = static_cast<int16_t>(kCx + kArcR * sinf(rad));
  *oy = static_cast<int16_t>(kCy + kArcR * cosf(rad) * rScale + yOff);
}

void diskAnimCb(void* var, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
}

void stopDiskAnim() {
  if (g_animRunning && g_disk) {
    lv_anim_del(g_disk, diskAnimCb);
    g_animRunning = false;
  }
}

void startDiskAnim(bool pulse) {
  if (!g_disk) return;
  stopDiskAnim();
  lv_anim_init(&g_diskAnim);
  lv_anim_set_var(&g_diskAnim, g_disk);
  lv_anim_set_exec_cb(&g_diskAnim, diskAnimCb);
  lv_anim_set_values(&g_diskAnim, 100, 255);
  lv_anim_set_time(&g_diskAnim, pulse ? 450 : 1400);
  lv_anim_set_playback_time(&g_diskAnim, pulse ? 450 : 1400);
  lv_anim_set_repeat_count(&g_diskAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_diskAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_diskAnim);
  g_animRunning = true;
}

void setMarkColor(uint32_t hex) {
  if (g_markBody) lv_obj_set_style_bg_color(g_markBody, lv_color_hex(hex), 0);
  if (g_markBar1) lv_obj_set_style_bg_color(g_markBar1, lv_color_hex(COL_BG), 0);
  if (g_markBar2) lv_obj_set_style_bg_color(g_markBar2, lv_color_hex(COL_BG), 0);
}

void showMark(bool on, uint32_t color) {
  if (!g_markCont) return;
  if (on) {
    lv_obj_clear_flag(g_markCont, LV_OBJ_FLAG_HIDDEN);
    setMarkColor(color);
  } else {
    lv_obj_add_flag(g_markCont, LV_OBJ_FLAG_HIDDEN);
  }
}

void showIcon(bool on, uint32_t color, const char* glyph) {
  if (!g_iconCircle) return;
  if (on) {
    lv_obj_clear_flag(g_iconCircle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(g_iconCircle, lv_color_hex(color), 0);
    if (g_iconLabel) lv_label_set_text(g_iconLabel, glyph);
  } else {
    lv_obj_add_flag(g_iconCircle, LV_OBJ_FLAG_HIDDEN);
  }
}

void showWorkArcs(bool on) {
  for (int i = 0; i < 3; ++i) {
    if (!g_workArcs[i]) continue;
    if (on) lv_obj_clear_flag(g_workArcs[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_workArcs[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void hideAllStateConts() {
  lv_obj_t* conts[] = {g_contBoot, g_contHome, g_contFleet, g_contConfirm,
                       g_contWorking, g_contDone, g_contError};
  for (lv_obj_t* c : conts) {
    if (c) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  }
}

uint32_t statusColorForText(const char* text) {
  if (text == nullptr) return COL_MUTED;
  if (std::strstr(text, "Work") || std::strstr(text, "work") ||
      std::strstr(text, "TRAB") || std::strstr(text, "erro") ||
      std::strstr(text, "ERROR") || std::strstr(text, "Error")) {
    return COL_RED;
  }
  if (std::strstr(text, "Idle") || std::strstr(text, "idle") ||
      std::strstr(text, "OK") || std::strstr(text, "done") ||
      std::strstr(text, "Done") || std::strstr(text, "online")) {
    return COL_GREEN;
  }
  return COL_MUTED;
}

void applyFleetSlots() {
  const char* names[3] = {g_slotNameA, g_slotNameB, g_slotNameC};
  const char* stats[3] = {g_slotA, g_slotB, g_slotC};
  for (int i = 0; i < 3; ++i) {
    if (g_fleetName[i]) lv_label_set_text(g_fleetName[i], names[i]);
    if (g_fleetStatus[i]) lv_label_set_text(g_fleetStatus[i], stats[i]);
    if (g_fleetDot[i]) {
      lv_obj_set_style_bg_color(g_fleetDot[i],
                                lv_color_hex(statusColorForText(stats[i])), 0);
    }
  }
}

void applyLabels() {
  if (g_titleLabel) lv_label_set_text(g_titleLabel, g_title);
  if (g_messageLabel) lv_label_set_text(g_messageLabel, g_message);
  applyFleetSlots();
}

void refreshStateVisuals() {
  hideAllStateConts();
  showWorkArcs(false);
  if (g_onlinePill) lv_obj_add_flag(g_onlinePill, LV_OBJ_FLAG_HIDDEN);
  if (g_fleetHint) lv_obj_add_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);
  if (g_errorRim) lv_obj_add_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);

  // default: show title/message mid band
  if (g_titleLabel) lv_obj_clear_flag(g_titleLabel, LV_OBJ_FLAG_HIDDEN);
  if (g_messageLabel) lv_obj_clear_flag(g_messageLabel, LV_OBJ_FLAG_HIDDEN);
  if (g_disk) lv_obj_clear_flag(g_disk, LV_OBJ_FLAG_HIDDEN);

  uint32_t ring = COL_IDLE;
  bool breathe = false;
  bool pulse = false;

  switch (g_state) {
    case UiState::BOOT:
      if (g_contBoot) lv_obj_clear_flag(g_contBoot, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateBoot));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      showMark(true, COL_RED);
      showIcon(false, COL_RED, "");
      ring = COL_IDLE;
      breathe = true;
      if (g_titleLabel) lv_obj_add_flag(g_titleLabel, LV_OBJ_FLAG_HIDDEN);
      if (g_messageLabel) lv_obj_add_flag(g_messageLabel, LV_OBJ_FLAG_HIDDEN);
      break;

    case UiState::HOME:
      if (g_contHome) lv_obj_clear_flag(g_contHome, LV_OBJ_FLAG_HIDDEN);
      if (g_onlinePill) lv_obj_clear_flag(g_onlinePill, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::TitleDefault));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      showMark(true, COL_RED);
      showIcon(false, COL_RED, "");
      ring = COL_IDLE;
      breathe = true;
      break;

    case UiState::FLEET_STATUS:
      if (g_contFleet) lv_obj_clear_flag(g_contFleet, LV_OBJ_FLAG_HIDDEN);
      if (g_fleetHint) lv_obj_clear_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateFleet));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      // smaller mark, hide mid title/message (cards carry info)
      showMark(true, COL_RED);
      showIcon(false, COL_RED, "");
      if (g_disk) lv_obj_add_flag(g_disk, LV_OBJ_FLAG_HIDDEN);
      if (g_titleLabel) lv_obj_add_flag(g_titleLabel, LV_OBJ_FLAG_HIDDEN);
      if (g_messageLabel) lv_obj_add_flag(g_messageLabel, LV_OBJ_FLAG_HIDDEN);
      ring = COL_LINE;
      break;

    case UiState::TOUCH_CONFIRM:
      if (g_contConfirm) lv_obj_clear_flag(g_contConfirm, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateTouchConfirm));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      showMark(false, COL_RED);
      showIcon(true, COL_GREEN, LV_SYMBOL_OK);
      ring = COL_GREEN;
      break;

    case UiState::WORKING:
      if (g_contWorking) lv_obj_clear_flag(g_contWorking, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateWorking));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_AMBER), 0);
      }
      showMark(true, COL_AMBER);
      showIcon(false, COL_RED, "");
      showWorkArcs(true);
      ring = COL_AMBER;
      pulse = true;
      break;

    case UiState::DONE:
      if (g_contDone) lv_obj_clear_flag(g_contDone, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateDone));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_GREEN), 0);
      }
      showMark(true, COL_GREEN);
      showIcon(false, COL_RED, "");
      ring = COL_GREEN;
      break;

    case UiState::ERROR:
      if (g_contError) lv_obj_clear_flag(g_contError, LV_OBJ_FLAG_HIDDEN);
      if (g_errorRim) lv_obj_clear_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateError));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_RED), 0);
      }
      showMark(false, COL_RED);
      showIcon(true, COL_RED, "!");
      ring = COL_RED;
      pulse = true;
      break;
  }

  if (g_disk) {
    lv_obj_set_style_border_color(g_disk, lv_color_hex(ring), 0);
    lv_obj_set_style_border_opa(g_disk, LV_OPA_COVER, 0);
  }

  if (breathe || pulse) {
    startDiskAnim(pulse);
  } else {
    stopDiskAnim();
    if (g_disk) lv_obj_set_style_border_opa(g_disk, LV_OPA_COVER, 0);
  }

  applyLabels();
}

bool gfxBegin() {
  g_bus = new Arduino_SWSPI(
      GFX_NOT_DEFINED /* DC */, GFX_NOT_DEFINED /* CS */,
      PIN_LCD_SCL /* SCK */, PIN_LCD_SDA /* MOSI */, GFX_NOT_DEFINED /* MISO */);

  g_rgbpanel = new Arduino_ESP32RGBPanel(
      PIN_LCD_DE, PIN_LCD_VSYNC, PIN_LCD_HSYNC, PIN_LCD_PCLK,
      PIN_LCD_R1, PIN_LCD_R2, PIN_LCD_R3, PIN_LCD_R4, PIN_LCD_R5,
      PIN_LCD_G0, PIN_LCD_G1, PIN_LCD_G2, PIN_LCD_G3, PIN_LCD_G4, PIN_LCD_G5,
      PIN_LCD_B1, PIN_LCD_B2, PIN_LCD_B3, PIN_LCD_B4, PIN_LCD_B5,
      1 /* hsync_pol */, 50 /* hfp */, 8 /* hpw */, 10 /* hbp */,
      1 /* vsync_pol */, 10 /* vfp */, 8 /* vpw */, 10 /* vbp */);

  g_gfx = new Arduino_RGB_Display(
      LCD_H_RES, LCD_V_RES, g_rgbpanel, 0 /* rotation */, true /* auto_flush */,
      g_bus, GFX_NOT_DEFINED /* RST */,
      st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

  panelHwReset();
  const bool ok = g_gfx->begin();
  tca9554SetPin(EXIO_LCD_CS, true);
  backlightOn();
  return ok;
}

lv_obj_t* makeStateCont(lv_obj_t* parent) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_set_size(c, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(c, 0, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  return c;
}

void buildGrokMark(lv_obj_t* parent) {
  g_markCont = lv_obj_create(parent);
  lv_obj_set_size(g_markCont, 72, 72);
  lv_obj_set_pos(g_markCont, kCx - 36, kDiskCy - 44);
  lv_obj_set_style_bg_opa(g_markCont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_markCont, 0, 0);
  lv_obj_set_style_pad_all(g_markCont, 0, 0);
  lv_obj_clear_flag(g_markCont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  // Approximate Grok pentagon as rounded diamond + bars
  g_markBody = lv_obj_create(g_markCont);
  lv_obj_set_size(g_markBody, 56, 56);
  lv_obj_align(g_markBody, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(g_markBody, 8, 0);
  lv_obj_set_style_bg_color(g_markBody, lv_color_hex(COL_RED), 0);
  lv_obj_set_style_bg_opa(g_markBody, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_markBody, 0, 0);
  lv_obj_set_style_pad_all(g_markBody, 0, 0);
  lv_obj_clear_flag(g_markBody, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  g_markBar1 = lv_obj_create(g_markBody);
  lv_obj_set_size(g_markBar1, 28, 3);
  lv_obj_align(g_markBar1, LV_ALIGN_CENTER, 0, -4);
  lv_obj_set_style_bg_color(g_markBar1, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_markBar1, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_markBar1, 0, 0);
  lv_obj_set_style_radius(g_markBar1, 1, 0);
  lv_obj_clear_flag(g_markBar1, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  g_markBar2 = lv_obj_create(g_markBody);
  lv_obj_set_size(g_markBar2, 20, 3);
  lv_obj_align(g_markBar2, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_bg_color(g_markBar2, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_markBar2, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_markBar2, 0, 0);
  lv_obj_set_style_radius(g_markBar2, 1, 0);
  lv_obj_clear_flag(g_markBar2, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

void buildCenterDisk(lv_obj_t* parent) {
  g_disk = lv_obj_create(parent);
  lv_obj_set_size(g_disk, kDiskR * 2, kDiskR * 2);
  lv_obj_set_pos(g_disk, kCx - kDiskR, kDiskCy - kDiskR);
  lv_obj_set_style_radius(g_disk, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_disk, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(g_disk, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_disk, 4, 0);
  lv_obj_set_style_border_color(g_disk, lv_color_hex(COL_IDLE), 0);
  lv_obj_set_style_pad_all(g_disk, 0, 0);
  lv_obj_clear_flag(g_disk, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  // Working arc hints (hidden by default)
  for (int i = 0; i < 3; ++i) {
    g_workArcs[i] = lv_arc_create(parent);
    lv_obj_set_size(g_workArcs[i], 140, 140);
    lv_obj_set_pos(g_workArcs[i], kCx - 70, kDiskCy - 70);
    lv_arc_set_bg_angles(g_workArcs[i], 0, 360);
    lv_arc_set_angles(g_workArcs[i], 20 + i * 120, 70 + i * 120);
    lv_obj_remove_style(g_workArcs[i], nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(g_workArcs[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(g_workArcs[i], 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_workArcs[i], lv_color_hex(COL_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(g_workArcs[i], LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(g_workArcs[i], LV_OBJ_FLAG_HIDDEN);
  }

  g_iconCircle = lv_obj_create(parent);
  lv_obj_set_size(g_iconCircle, 72, 72);
  lv_obj_set_pos(g_iconCircle, kCx - 36, kDiskCy - 36);
  lv_obj_set_style_radius(g_iconCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_iconCircle, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_bg_opa(g_iconCircle, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_iconCircle, 0, 0);
  lv_obj_set_style_pad_all(g_iconCircle, 0, 0);
  lv_obj_clear_flag(g_iconCircle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_iconCircle, LV_OBJ_FLAG_HIDDEN);

  g_iconLabel = lv_label_create(g_iconCircle);
  lv_label_set_text(g_iconLabel, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(g_iconLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(g_iconLabel, lv_color_hex(COL_BG), 0);
  lv_obj_center(g_iconLabel);

  buildGrokMark(parent);
}

void buildFleetCard(lv_obj_t* parent, int idx, float angleDeg, const char* letter) {
  int16_t bx, by;
  const float rad = angleDeg * kPi / 180.0f;
  const float r = 155.0f;
  bx = static_cast<int16_t>(kCx + r * sinf(rad));
  by = static_cast<int16_t>(kCy + 55 + r * cosf(rad) * 0.35f);

  lv_obj_t* card = lv_btn_create(parent);
  lv_obj_set_size(card, 140, 84);
  lv_obj_set_pos(card, bx - 70, by - 42);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(COL_LINE), 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 8, 0);

  TouchZone zone = TouchZone::Z_SLOT_A;
  if (idx == 1) zone = TouchZone::Z_SLOT_B;
  if (idx == 2) zone = TouchZone::Z_SLOT_C;
  lv_obj_add_event_cb(card, onBtnClicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(zone)));

  lv_obj_t* let = makeLabel(card, &lv_font_montserrat_14, lv_color_hex(COL_RED), 0);
  lv_label_set_text(let, letter);
  lv_obj_align(let, LV_ALIGN_TOP_LEFT, 0, 0);

  g_fleetName[idx] = makeLabel(card, &lv_font_montserrat_14, lv_color_hex(COL_TEXT), 120);
  lv_obj_align(g_fleetName[idx], LV_ALIGN_TOP_MID, 0, 18);

  g_fleetDot[idx] = lv_obj_create(card);
  lv_obj_set_size(g_fleetDot[idx], 10, 10);
  lv_obj_set_style_radius(g_fleetDot[idx], LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_fleetDot[idx], lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_bg_opa(g_fleetDot[idx], LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_fleetDot[idx], 0, 0);
  lv_obj_clear_flag(g_fleetDot[idx], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(g_fleetDot[idx], LV_ALIGN_BOTTOM_LEFT, 8, -4);

  g_fleetStatus[idx] = makeLabel(card, &lv_font_montserrat_12, lv_color_hex(COL_MUTED), 90);
  lv_obj_align(g_fleetStatus[idx], LV_ALIGN_BOTTOM_LEFT, 24, -2);

  g_fleetCard[idx] = card;
}

void buildUi() {
  g_screen = lv_scr_act();
  lv_obj_set_style_bg_color(g_screen, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Soft outer rim
  lv_obj_t* rim = lv_obj_create(g_screen);
  lv_obj_set_size(rim, 472, 472);
  lv_obj_center(rim);
  lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(rim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rim, 2, 0);
  lv_obj_set_style_border_color(rim, lv_color_hex(COL_LINE), 0);
  lv_obj_clear_flag(rim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  // ERROR outer accent rim
  g_errorRim = lv_obj_create(g_screen);
  lv_obj_set_size(g_errorRim, 464, 464);
  lv_obj_center(g_errorRim);
  lv_obj_set_style_radius(g_errorRim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_errorRim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_errorRim, 6, 0);
  lv_obj_set_style_border_color(g_errorRim, lv_color_hex(COL_RED), 0);
  lv_obj_clear_flag(g_errorRim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);

  // Header y≈40–70
  g_headerLabel = makeLabel(g_screen, &lv_font_montserrat_12, lv_color_hex(COL_MUTED), 300);
  lv_obj_align(g_headerLabel, LV_ALIGN_TOP_MID, 0, 48);

  buildCenterDisk(g_screen);

  // Mid y≈300–340 title + message
  g_titleLabel = makeLabel(g_screen, &lv_font_montserrat_22, lv_color_hex(COL_TEXT), 360);
  lv_obj_align(g_titleLabel, LV_ALIGN_TOP_MID, 0, 300);

  g_messageLabel = makeLabel(g_screen, &lv_font_montserrat_14, lv_color_hex(COL_MUTED), 360);
  lv_obj_align(g_messageLabel, LV_ALIGN_TOP_MID, 0, 328);

  // Online pill (HOME)
  g_onlinePill = lv_obj_create(g_screen);
  lv_obj_set_size(g_onlinePill, 96, 24);
  lv_obj_set_pos(g_onlinePill, kCx - 48, 360);
  lv_obj_set_style_radius(g_onlinePill, 12, 0);
  lv_obj_set_style_bg_color(g_onlinePill, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(g_onlinePill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_onlinePill, 0, 0);
  lv_obj_set_style_pad_all(g_onlinePill, 0, 0);
  lv_obj_clear_flag(g_onlinePill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* odot = lv_obj_create(g_onlinePill);
  lv_obj_set_size(odot, 10, 10);
  lv_obj_set_pos(odot, 10, 7);
  lv_obj_set_style_radius(odot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(odot, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_bg_opa(odot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(odot, 0, 0);
  lv_obj_clear_flag(odot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* olab = makeLabel(g_onlinePill, &lv_font_montserrat_12, lv_color_hex(COL_TEXT), 0);
  lv_label_set_text(olab, i18nStr(I18nId::Online));
  lv_obj_set_pos(olab, 28, 4);
  lv_obj_add_flag(g_onlinePill, LV_OBJ_FLAG_HIDDEN);

  // Fleet hint
  g_fleetHint = makeLabel(g_screen, &lv_font_montserrat_12, lv_color_hex(COL_DIM), 200);
  lv_label_set_text(g_fleetHint, i18nStr(I18nId::TouchSlotHint));
  lv_obj_align(g_fleetHint, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_add_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);

  // --- per-state button containers ---
  g_contBoot = makeStateCont(g_screen);
  g_contHome = makeStateCont(g_screen);
  g_contFleet = makeStateCont(g_screen);
  g_contConfirm = makeStateCont(g_screen);
  g_contWorking = makeStateCont(g_screen);
  g_contDone = makeStateCont(g_screen);
  g_contError = makeStateCont(g_screen);

  // HOME: A / B / C
  {
    int16_t x, y;
    arcPos(-50.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotA), false, TouchZone::Z_SLOT_A, x, y);
    arcPos(0.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotB), false, TouchZone::Z_SLOT_B, x, y);
    arcPos(50.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotC), false, TouchZone::Z_SLOT_C, x, y);
  }

  // FLEET: 3 arc cards + small fleet label under mark
  {
    lv_obj_t* flab = makeLabel(g_contFleet, &lv_font_montserrat_12, lv_color_hex(COL_DIM), 0);
    lv_label_set_text(flab, i18nStr(I18nId::FleetLabel));
    lv_obj_set_pos(flab, kCx - 20, kDiskCy + 20);

    // compact mark on fleet (reuse shared mark; positioned via showMark)
    buildFleetCard(g_contFleet, 0, -55.f, "A");
    buildFleetCard(g_contFleet, 1, 0.f, "B");
    buildFleetCard(g_contFleet, 2, 55.f, "C");
  }

  // CONFIRM: Back / OK
  {
    int16_t x, y;
    arcPos(-40.f, &x, &y);
    makePill(g_contConfirm, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
    arcPos(40.f, &x, &y);
    makePill(g_contConfirm, i18nStr(I18nId::ActionOk), true, TouchZone::Z_ACTION_PRIMARY, x, y);
  }

  // WORKING: Back
  {
    int16_t x, y;
    arcPos(0.f, &x, &y);
    makePill(g_contWorking, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
  }

  // DONE: Home
  {
    int16_t x, y;
    arcPos(0.f, &x, &y);
    makePill(g_contDone, i18nStr(I18nId::ActionHome), false, TouchZone::Z_ACTION_BACK, x, y);
  }

  // ERROR: Back / Retry
  {
    int16_t x, y;
    arcPos(-40.f, &x, &y);
    makePill(g_contError, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
    arcPos(40.f, &x, &y);
    makePill(g_contError, i18nStr(I18nId::ActionRetry), true, TouchZone::Z_ACTION_PRIMARY, x, y);
  }

  // Seed defaults from i18n
  std::strncpy(g_title, i18nStr(I18nId::TitleIdle), sizeof(g_title) - 1);
  std::strncpy(g_message, i18nStr(I18nId::MsgTouchOrMqtt), sizeof(g_message) - 1);
  std::strncpy(g_slotNameA, i18nStr(I18nId::SlotNameA), sizeof(g_slotNameA) - 1);
  std::strncpy(g_slotNameB, i18nStr(I18nId::SlotNameB), sizeof(g_slotNameB) - 1);
  std::strncpy(g_slotNameC, i18nStr(I18nId::SlotNameC), sizeof(g_slotNameC) - 1);
  std::strncpy(g_slotA, i18nStr(I18nId::SlotStatusIdle), sizeof(g_slotA) - 1);
  std::strncpy(g_slotB, i18nStr(I18nId::SlotStatusWorking), sizeof(g_slotB) - 1);
  std::strncpy(g_slotC, i18nStr(I18nId::SlotStatusStandby), sizeof(g_slotC) - 1);
}

}  // namespace

const char* uiStateName(UiState s) {
  switch (s) {
    case UiState::BOOT:          return "BOOT";
    case UiState::HOME:          return "HOME";
    case UiState::FLEET_STATUS:  return "FLEET_STATUS";
    case UiState::TOUCH_CONFIRM: return "TOUCH_CONFIRM";
    case UiState::WORKING:       return "WORKING";
    case UiState::DONE:          return "DONE";
    case UiState::ERROR:         return "ERROR";
    default:                     return "?";
  }
}

void displayInit() {
  g_state = UiState::BOOT;
  g_lastLogged = static_cast<UiState>(0xFF);

  const bool ok = gfxBegin();
  Serial.print(F("[display] RGB ST7701 begin="));
  Serial.println(ok ? F("ok") : F("fail"));

  lv_init();

  const size_t bufPx = static_cast<size_t>(LCD_H_RES) * kBufLines;
  g_colorBuf = static_cast<lv_color_t*>(
      heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (g_colorBuf == nullptr) {
    g_colorBuf = static_cast<lv_color_t*>(malloc(bufPx * sizeof(lv_color_t)));
  }
  lv_disp_draw_buf_init(&g_drawBuf, g_colorBuf, nullptr, static_cast<uint32_t>(bufPx));

  lv_disp_drv_init(&g_dispDrv);
  g_dispDrv.hor_res = LCD_H_RES;
  g_dispDrv.ver_res = LCD_V_RES;
  g_dispDrv.flush_cb = flushCb;
  g_dispDrv.draw_buf = &g_drawBuf;
  lv_disp_drv_register(&g_dispDrv);

  buildUi();
  displaySetState(UiState::HOME);
  Serial.println(F("[display] UI v0.2 screens ready"));
}

void displaySetState(UiState s) {
  g_state = s;
  // Seed state-specific default title/message when still at scaffold defaults
  if (s == UiState::TOUCH_CONFIRM) {
    if (std::strcmp(g_title, i18nStr(I18nId::TitleIdle)) == 0 ||
        std::strcmp(g_title, "Grok Bot") == 0) {
      std::strncpy(g_title, i18nStr(I18nId::TitleWebhookSent), sizeof(g_title) - 1);
      std::strncpy(g_message, i18nStr(I18nId::MsgActionCompleted), sizeof(g_message) - 1);
    }
  } else if (s == UiState::DONE) {
    if (std::strcmp(g_title, i18nStr(I18nId::TitleIdle)) == 0) {
      std::strncpy(g_title, i18nStr(I18nId::TitleDoneDefault), sizeof(g_title) - 1);
      std::strncpy(g_message, i18nStr(I18nId::MsgDoneDefault), sizeof(g_message) - 1);
    }
  } else if (s == UiState::ERROR) {
    if (std::strcmp(g_title, i18nStr(I18nId::TitleIdle)) == 0) {
      std::strncpy(g_title, i18nStr(I18nId::TitleErrorDefault), sizeof(g_title) - 1);
      std::strncpy(g_message, i18nStr(I18nId::MsgTouchRetry), sizeof(g_message) - 1);
    }
  } else if (s == UiState::HOME) {
    // restore idle copy if coming home without MQTT override lingering oddly
    // keep current title/message from mqtt; only seed if empty
    if (g_title[0] == '\0') {
      std::strncpy(g_title, i18nStr(I18nId::TitleIdle), sizeof(g_title) - 1);
    }
  }
  refreshStateVisuals();
}

UiState displayGetState() { return g_state; }

void displaySetTitle(const char* title) {
  if (title == nullptr) return;
  std::strncpy(g_title, title, sizeof(g_title) - 1);
  g_title[sizeof(g_title) - 1] = '\0';
  applyLabels();
}

void displaySetMessage(const char* message) {
  if (message == nullptr) return;
  std::strncpy(g_message, message, sizeof(g_message) - 1);
  g_message[sizeof(g_message) - 1] = '\0';
  applyLabels();
}

void displaySetSlot(char slot, const char* text) {
  if (text == nullptr) return;
  char* dest = nullptr;
  size_t n = 0;
  char* nameDest = nullptr;
  size_t nameN = 0;
  if (slot == 'A' || slot == 'a') {
    dest = g_slotA;
    n = sizeof(g_slotA);
    nameDest = g_slotNameA;
    nameN = sizeof(g_slotNameA);
  } else if (slot == 'B' || slot == 'b') {
    dest = g_slotB;
    n = sizeof(g_slotB);
    nameDest = g_slotNameB;
    nameN = sizeof(g_slotNameB);
  } else if (slot == 'C' || slot == 'c') {
    dest = g_slotC;
    n = sizeof(g_slotC);
    nameDest = g_slotNameC;
    nameN = sizeof(g_slotNameC);
  } else {
    return;
  }

  // Accept "Name|Status" or plain status text
  const char* bar = std::strchr(text, '|');
  if (bar != nullptr && nameDest != nullptr) {
    const size_t nameLen = static_cast<size_t>(bar - text);
    const size_t copyN = nameLen < nameN - 1 ? nameLen : nameN - 1;
    std::memcpy(nameDest, text, copyN);
    nameDest[copyN] = '\0';
    std::strncpy(dest, bar + 1, n - 1);
    dest[n - 1] = '\0';
  } else {
    std::strncpy(dest, text, n - 1);
    dest[n - 1] = '\0';
  }
  applyLabels();
}

void displaySetAnim(const char* anim, char slotOpt) {
  if (anim == nullptr) return;
  if (std::strcmp(anim, "idle") == 0) {
    displaySetState(UiState::HOME);
  } else if (std::strcmp(anim, "working") == 0) {
    displaySetState(UiState::WORKING);
  } else if (std::strcmp(anim, "done") == 0) {
    displaySetState(UiState::DONE);
  } else if (std::strcmp(anim, "error") == 0) {
    displaySetState(UiState::ERROR);
  }
  if (slotOpt != '\0') {
    displaySetSlot(slotOpt, anim);
  }
}

void displayTick() {
  if (g_state != g_lastLogged) {
    Serial.print(F("[display] state="));
    Serial.println(uiStateName(g_state));
    g_lastLogged = g_state;
  }
  lv_timer_handler();
}

// Called from touch.cpp zone path so LVGL + CST820 share debounce with buttons
void displayFireZone(TouchZone zone) { fireZone(zone); }
