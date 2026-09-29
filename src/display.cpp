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

// --- palette ---
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

// App avatar colors (v0.4)
constexpr uint32_t COL_GROKU      = 0x7AD35A;
constexpr uint32_t COL_GROKU_DOT  = 0x4FA83A;
constexpr uint32_t COL_ESPFORGE   = 0x2EC4B6;
constexpr uint32_t COL_ENGDIR     = 0xE23B3B;
constexpr uint32_t COL_PRINTMAKER = 0xF0A020;
constexpr uint32_t COL_EYE        = 0x0C0C0E;
constexpr uint32_t COL_BADGE_OK   = 0x2EC4B6;  // teal check
constexpr uint32_t COL_BADGE_ERR  = 0xE11D2E;

constexpr int16_t kCx = 240;
constexpr int16_t kCy = 240;
constexpr int16_t kDiskR = 88;       // Ø≈176
constexpr int16_t kDiskCy = 220;     // center of disk / avatar
constexpr int16_t kArcR = 165;
constexpr float kPi = 3.14159265f;

constexpr int16_t kAvSize = 96;      // main avatar body box
constexpr int16_t kAvBaseY = kDiskCy - kAvSize / 2;
constexpr int16_t kAvBaseX = kCx - kAvSize / 2;
constexpr int16_t kMiniAv = 32;

enum class AvatarKind : uint8_t {
  Groku = 0,
  EspForge,
  EngDir,
  PrintMaker,
};

enum class BadgeKind : uint8_t {
  None = 0,
  Check,
  Cross,
};

Arduino_DataBus* g_bus = nullptr;
Arduino_ESP32RGBPanel* g_rgbpanel = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;

UiState g_state = UiState::BOOT;
UiState g_lastLogged = static_cast<UiState>(0xFF);

char g_title[40] = "Groku CEO";
char g_message[80] = "idle";
char g_slotA[32] = "Idle";
char g_slotB[32] = "Working";
char g_slotC[32] = "Stand by";
char g_slotNameA[24] = "PrintMaker";
char g_slotNameB[24] = "EspForge";
char g_slotNameC[24] = "EngDir";

lv_disp_draw_buf_t g_drawBuf;
lv_disp_drv_t g_dispDrv;
lv_color_t* g_colorBuf = nullptr;
constexpr int kBufLines = 40;

lv_obj_t* g_screen = nullptr;
lv_obj_t* g_headerLabel = nullptr;
lv_obj_t* g_titleLabel = nullptr;
lv_obj_t* g_messageLabel = nullptr;

lv_obj_t* g_disk = nullptr;
lv_obj_t* g_workArcs[3] = {nullptr, nullptr, nullptr};
lv_obj_t* g_fleetHint = nullptr;
lv_obj_t* g_onlinePill = nullptr;
lv_obj_t* g_errorRim = nullptr;
lv_obj_t* g_halo = nullptr;

// Main center avatar (replaces red geometric mark)
lv_obj_t* g_avatarCont = nullptr;
lv_obj_t* g_avBody[4] = {nullptr, nullptr, nullptr, nullptr};  // per kind root
lv_obj_t* g_avEyeL = nullptr;
lv_obj_t* g_avEyeR = nullptr;
lv_obj_t* g_avBadge = nullptr;
lv_obj_t* g_avBadgeLabel = nullptr;
AvatarKind g_avKind = AvatarKind::Groku;
int16_t g_avPosX = kAvBaseX;
int16_t g_avPosY = kAvBaseY;
int16_t g_avCurSize = kAvSize;

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
lv_obj_t* g_fleetMini[3] = {nullptr, nullptr, nullptr};

lv_anim_t g_diskAnim;
lv_anim_t g_scaleAnim;
lv_anim_t g_haloAnim;
lv_anim_t g_arcAnim;
lv_anim_t g_buzzAnim;
lv_anim_t g_shakeAnim;
lv_anim_t g_fleetAnim;
lv_anim_t g_popAnim;
lv_anim_t g_bobAnim;
lv_anim_t g_blinkAnim;
lv_anim_t g_bounceAnim;
bool g_animRunning = false;
int g_fleetPulseIdx = -1;
uint32_t g_lastZoneUiMs = 0;

constexpr int16_t kDiskDiam = kDiskR * 2;
constexpr int16_t kHaloPad = 14;

void styleBare(lv_obj_t* o) {
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

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

void diskBorderOpaCb(void* var, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
}

void diskScaleCb(void* var, int32_t v) {
  lv_obj_t* disk = static_cast<lv_obj_t*>(var);
  lv_obj_set_size(disk, v, v);
  lv_obj_set_pos(disk, kCx - v / 2, kDiskCy - v / 2);
}

void haloOpaCb(void* var, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
}

void workArcSpinCb(void* /*var*/, int32_t v) {
  for (int i = 0; i < 3; ++i) {
    if (!g_workArcs[i]) continue;
    const int32_t start = 20 + i * 120 + v;
    const int32_t end = 70 + i * 120 + v;
    lv_arc_set_angles(g_workArcs[i], start, end);
  }
}

void borderWidthCb(void* var, int32_t v) {
  lv_obj_set_style_border_width(static_cast<lv_obj_t*>(var), static_cast<lv_coord_t>(v), 0);
}

void fleetBorderOpaCb(void* var, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
}

void avatarBobCb(void* /*var*/, int32_t v) {
  if (!g_avatarCont) return;
  g_avPosY = kAvBaseY + static_cast<int16_t>(v);
  lv_obj_set_pos(g_avatarCont, g_avPosX, g_avPosY);
}

void avatarBounceCb(void* /*var*/, int32_t v) {
  if (!g_avatarCont) return;
  g_avPosY = kAvBaseY + static_cast<int16_t>(v);
  lv_obj_set_pos(g_avatarCont, g_avPosX, g_avPosY);
}

void avatarShakeCb(void* /*var*/, int32_t v) {
  if (!g_avatarCont) return;
  g_avPosX = kAvBaseX + static_cast<int16_t>(v);
  lv_obj_set_pos(g_avatarCont, g_avPosX, g_avPosY);
}

void avatarPopCb(void* /*var*/, int32_t v) {
  if (!g_avatarCont) return;
  g_avCurSize = static_cast<int16_t>(v);
  const int16_t x = kCx - g_avCurSize / 2;
  const int16_t y = kDiskCy - g_avCurSize / 2;
  g_avPosX = x;
  g_avPosY = y;
  lv_obj_set_size(g_avatarCont, g_avCurSize, g_avCurSize);
  lv_obj_set_pos(g_avatarCont, x, y);
}

void eyeBlinkCb(void* /*var*/, int32_t v) {
  // v: eye height 1..5; brief close
  const lv_coord_t h = static_cast<lv_coord_t>(v);
  if (g_avEyeL) {
    lv_obj_set_height(g_avEyeL, h);
    lv_obj_align(g_avEyeL, LV_ALIGN_CENTER, -14, -10);
  }
  if (g_avEyeR) {
    lv_obj_set_height(g_avEyeR, h);
    lv_obj_align(g_avEyeR, LV_ALIGN_CENTER, 14, -10);
  }
}

void resetDiskGeometry() {
  if (g_disk) {
    lv_obj_set_size(g_disk, kDiskDiam, kDiskDiam);
    lv_obj_set_pos(g_disk, kCx - kDiskR, kDiskCy - kDiskR);
    lv_obj_set_style_border_width(g_disk, 4, 0);
    lv_obj_set_style_border_opa(g_disk, LV_OPA_COVER, 0);
  }
  if (g_halo) {
    const int16_t hr = kDiskR + kHaloPad;
    lv_obj_set_size(g_halo, hr * 2, hr * 2);
    lv_obj_set_pos(g_halo, kCx - hr, kDiskCy - hr);
    lv_obj_set_style_border_opa(g_halo, LV_OPA_30, 0);
  }
  if (g_avatarCont) {
    g_avCurSize = kAvSize;
    g_avPosX = kAvBaseX;
    g_avPosY = kAvBaseY;
    lv_obj_set_size(g_avatarCont, kAvSize, kAvSize);
    lv_obj_set_pos(g_avatarCont, g_avPosX, g_avPosY);
    lv_obj_set_style_transform_zoom(g_avatarCont, LV_IMG_ZOOM_NONE, 0);
    lv_obj_set_style_transform_angle(g_avatarCont, 0, 0);
  }
  if (g_avEyeL) {
    lv_obj_set_size(g_avEyeL, 12, 5);
    lv_obj_align(g_avEyeL, LV_ALIGN_CENTER, -14, -10);
    lv_obj_set_style_opa(g_avEyeL, LV_OPA_COVER, 0);
  }
  if (g_avEyeR) {
    lv_obj_set_size(g_avEyeR, 12, 5);
    lv_obj_align(g_avEyeR, LV_ALIGN_CENTER, 14, -10);
    lv_obj_set_style_opa(g_avEyeR, LV_OPA_COVER, 0);
  }
}

void resetFleetCardBorders() {
  for (int i = 0; i < 3; ++i) {
    if (!g_fleetCard[i]) continue;
    lv_obj_set_style_border_color(g_fleetCard[i], lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_border_opa(g_fleetCard[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_fleetCard[i], 2, 0);
  }
  g_fleetPulseIdx = -1;
}

void stopAllAnims() {
  if (g_disk) {
    lv_anim_del(g_disk, diskBorderOpaCb);
    lv_anim_del(g_disk, diskScaleCb);
    lv_anim_del(g_disk, borderWidthCb);
  }
  if (g_halo) lv_anim_del(g_halo, haloOpaCb);
  if (g_workArcs[0]) lv_anim_del(g_workArcs[0], workArcSpinCb);
  if (g_avatarCont) {
    lv_anim_del(g_avatarCont, avatarBobCb);
    lv_anim_del(g_avatarCont, avatarBounceCb);
    lv_anim_del(g_avatarCont, avatarShakeCb);
    lv_anim_del(g_avatarCont, avatarPopCb);
  }
  if (g_avEyeL) lv_anim_del(g_avEyeL, eyeBlinkCb);
  if (g_fleetPulseIdx >= 0 && g_fleetPulseIdx < 3 && g_fleetCard[g_fleetPulseIdx]) {
    lv_anim_del(g_fleetCard[g_fleetPulseIdx], fleetBorderOpaCb);
  }
  g_animRunning = false;
}

void startBorderOpaAnim(lv_obj_t* obj, uint32_t ms, int32_t lo, int32_t hi) {
  if (!obj) return;
  lv_anim_init(&g_diskAnim);
  lv_anim_set_var(&g_diskAnim, obj);
  lv_anim_set_exec_cb(&g_diskAnim, diskBorderOpaCb);
  lv_anim_set_values(&g_diskAnim, lo, hi);
  lv_anim_set_time(&g_diskAnim, ms);
  lv_anim_set_playback_time(&g_diskAnim, ms);
  lv_anim_set_repeat_count(&g_diskAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_diskAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_diskAnim);
  g_animRunning = true;
}

void startIdleBobBlink() {
  if (!g_avatarCont) return;
  // Vertical bob ±5px ~1.4s
  lv_anim_init(&g_bobAnim);
  lv_anim_set_var(&g_bobAnim, g_avatarCont);
  lv_anim_set_exec_cb(&g_bobAnim, avatarBobCb);
  lv_anim_set_values(&g_bobAnim, -5, 5);
  lv_anim_set_time(&g_bobAnim, 1400);
  lv_anim_set_playback_time(&g_bobAnim, 1400);
  lv_anim_set_repeat_count(&g_bobAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_bobAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_bobAnim);

  // Blink: eye height 5→1→5, long gap between blinks
  if (g_avEyeL) {
    lv_anim_init(&g_blinkAnim);
    lv_anim_set_var(&g_blinkAnim, g_avEyeL);
    lv_anim_set_exec_cb(&g_blinkAnim, eyeBlinkCb);
    lv_anim_set_values(&g_blinkAnim, 5, 1);
    lv_anim_set_time(&g_blinkAnim, 90);
    lv_anim_set_playback_time(&g_blinkAnim, 90);
    lv_anim_set_repeat_count(&g_blinkAnim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&g_blinkAnim, 2800);
    lv_anim_set_path_cb(&g_blinkAnim, lv_anim_path_ease_in_out);
    lv_anim_start(&g_blinkAnim);
  }
  g_animRunning = true;
}

void startIdleBreathe() {
  if (!g_disk) return;
  startBorderOpaAnim(g_disk, 1400, 110, 255);
  lv_anim_init(&g_scaleAnim);
  lv_anim_set_var(&g_scaleAnim, g_disk);
  lv_anim_set_exec_cb(&g_scaleAnim, diskScaleCb);
  lv_anim_set_values(&g_scaleAnim, kDiskDiam, kDiskDiam + 8);
  lv_anim_set_time(&g_scaleAnim, 1400);
  lv_anim_set_playback_time(&g_scaleAnim, 1400);
  lv_anim_set_repeat_count(&g_scaleAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_scaleAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_scaleAnim);
  if (g_halo) {
    lv_obj_clear_flag(g_halo, LV_OBJ_FLAG_HIDDEN);
    lv_anim_init(&g_haloAnim);
    lv_anim_set_var(&g_haloAnim, g_halo);
    lv_anim_set_exec_cb(&g_haloAnim, haloOpaCb);
    lv_anim_set_values(&g_haloAnim, 40, 120);
    lv_anim_set_time(&g_haloAnim, 1400);
    lv_anim_set_playback_time(&g_haloAnim, 1400);
    lv_anim_set_repeat_count(&g_haloAnim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&g_haloAnim, lv_anim_path_ease_in_out);
    lv_anim_start(&g_haloAnim);
  }
  startIdleBobBlink();
}

void startWorkingAnims() {
  if (!g_disk) return;
  startBorderOpaAnim(g_disk, 450, 90, 255);
  if (g_workArcs[0]) {
    lv_anim_init(&g_arcAnim);
    lv_anim_set_var(&g_arcAnim, g_workArcs[0]);
    lv_anim_set_exec_cb(&g_arcAnim, workArcSpinCb);
    lv_anim_set_values(&g_arcAnim, 0, 360);
    lv_anim_set_time(&g_arcAnim, 2400);
    lv_anim_set_repeat_count(&g_arcAnim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&g_arcAnim, lv_anim_path_linear);
    lv_anim_start(&g_arcAnim);
  }
  // Bounce avatar ±8px faster
  if (g_avatarCont) {
    lv_anim_init(&g_bounceAnim);
    lv_anim_set_var(&g_bounceAnim, g_avatarCont);
    lv_anim_set_exec_cb(&g_bounceAnim, avatarBounceCb);
    lv_anim_set_values(&g_bounceAnim, -8, 6);
    lv_anim_set_time(&g_bounceAnim, 380);
    lv_anim_set_playback_time(&g_bounceAnim, 380);
    lv_anim_set_repeat_count(&g_bounceAnim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&g_bounceAnim, lv_anim_path_ease_in_out);
    lv_anim_start(&g_bounceAnim);
  }
  g_animRunning = true;
}

void startPopScale() {
  if (!g_avatarCont) return;
  lv_anim_init(&g_popAnim);
  lv_anim_set_var(&g_popAnim, g_avatarCont);
  lv_anim_set_exec_cb(&g_popAnim, avatarPopCb);
  lv_anim_set_values(&g_popAnim, kAvSize - 18, kAvSize);
  lv_anim_set_time(&g_popAnim, 420);
  lv_anim_set_playback_time(&g_popAnim, 0);
  lv_anim_set_repeat_count(&g_popAnim, 0);
  lv_anim_set_path_cb(&g_popAnim, lv_anim_path_overshoot);
  lv_anim_start(&g_popAnim);
  g_animRunning = true;
}

void startErrorAnims() {
  if (!g_disk) return;
  lv_anim_init(&g_buzzAnim);
  lv_anim_set_var(&g_buzzAnim, g_disk);
  lv_anim_set_exec_cb(&g_buzzAnim, borderWidthCb);
  lv_anim_set_values(&g_buzzAnim, 3, 10);
  lv_anim_set_time(&g_buzzAnim, 90);
  lv_anim_set_playback_time(&g_buzzAnim, 90);
  lv_anim_set_repeat_count(&g_buzzAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_buzzAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_buzzAnim);
  if (g_avatarCont) {
    lv_anim_init(&g_shakeAnim);
    lv_anim_set_var(&g_shakeAnim, g_avatarCont);
    lv_anim_set_exec_cb(&g_shakeAnim, avatarShakeCb);
    lv_anim_set_values(&g_shakeAnim, -5, 5);
    lv_anim_set_time(&g_shakeAnim, 70);
    lv_anim_set_playback_time(&g_shakeAnim, 70);
    lv_anim_set_repeat_count(&g_shakeAnim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&g_shakeAnim, lv_anim_path_ease_in_out);
    lv_anim_start(&g_shakeAnim);
  }
  g_animRunning = true;
}

int findWorkingFleetSlot() {
  const char* stats[3] = {g_slotA, g_slotB, g_slotC};
  for (int i = 0; i < 3; ++i) {
    if (stats[i] == nullptr) continue;
    if (std::strstr(stats[i], "Work") || std::strstr(stats[i], "work") ||
        std::strstr(stats[i], "TRAB") || std::strstr(stats[i], "Trab")) {
      return i;
    }
  }
  return 1;
}

void startFleetCardPulse() {
  resetFleetCardBorders();
  const int idx = findWorkingFleetSlot();
  if (idx < 0 || idx > 2 || !g_fleetCard[idx]) return;
  g_fleetPulseIdx = idx;
  lv_obj_set_style_border_color(g_fleetCard[idx], lv_color_hex(COL_AMBER), 0);
  lv_obj_set_style_border_width(g_fleetCard[idx], 3, 0);
  lv_anim_init(&g_fleetAnim);
  lv_anim_set_var(&g_fleetAnim, g_fleetCard[idx]);
  lv_anim_set_exec_cb(&g_fleetAnim, fleetBorderOpaCb);
  lv_anim_set_values(&g_fleetAnim, 80, 255);
  lv_anim_set_time(&g_fleetAnim, 550);
  lv_anim_set_playback_time(&g_fleetAnim, 550);
  lv_anim_set_repeat_count(&g_fleetAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&g_fleetAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&g_fleetAnim);
  g_animRunning = true;
}

void addSlantEyes(lv_obj_t* parent, int16_t eyeW, int16_t eyeH, int16_t gap,
                  int16_t yOff) {
  // Both eyes tilted \\ (~20°) — short rounded rects, not dots
  auto makeEye = [&](int16_t xOff) -> lv_obj_t* {
    lv_obj_t* e = lv_obj_create(parent);
    lv_obj_set_size(e, eyeW, eyeH);
    styleBare(e);
    lv_obj_set_style_radius(e, eyeH / 2, 0);
    lv_obj_set_style_bg_color(e, lv_color_hex(COL_EYE), 0);
    lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(e, eyeW / 2, 0);
    lv_obj_set_style_transform_pivot_y(e, eyeH / 2, 0);
    lv_obj_set_style_transform_angle(e, 200, 0);  // 20.0°
    lv_obj_align(e, LV_ALIGN_CENTER, xOff, yOff);
    return e;
  };
  makeEye(static_cast<int16_t>(-gap));
  makeEye(static_cast<int16_t>(gap));
}

lv_obj_t* buildBodyGroku(lv_obj_t* parent, int16_t box) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, box, box);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  styleBare(root);
  lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  const int16_t body = static_cast<int16_t>(box * 78 / 96);
  lv_obj_t* circle = lv_obj_create(root);
  lv_obj_set_size(circle, body, body);
  lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
  styleBare(circle);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, lv_color_hex(COL_GROKU), 0);
  lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);

  const int16_t dot = static_cast<int16_t>(box * 22 / 96);
  lv_obj_t* d = lv_obj_create(root);
  lv_obj_set_size(d, dot, dot);
  styleBare(d);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, lv_color_hex(COL_GROKU_DOT), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  lv_obj_align(d, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

  return root;
}

lv_obj_t* buildBodyHex(lv_obj_t* parent, int16_t box, uint32_t color) {
  // Pointy-top hex ≈ 3 overlapping rounded rects at 0°/60°/120°
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, box, box);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  styleBare(root);
  lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  const int16_t s = static_cast<int16_t>(box * 70 / 96);
  const int16_t h = static_cast<int16_t>(s * 58 / 70);
  for (int i = 0; i < 3; ++i) {
    lv_obj_t* r = lv_obj_create(root);
    lv_obj_set_size(r, s, h);
    styleBare(r);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(r, s / 2, 0);
    lv_obj_set_style_transform_pivot_y(r, h / 2, 0);
    lv_obj_set_style_transform_angle(r, i * 600, 0);  // 0, 60, 120 deg
    lv_obj_align(r, LV_ALIGN_CENTER, 0, 0);
  }
  return root;
}

lv_obj_t* buildBodyTear(lv_obj_t* parent, int16_t box, uint32_t color) {
  // Teardrop ≈ circle + 45° diamond (point) on top
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, box, box);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  styleBare(root);
  lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  const int16_t body = static_cast<int16_t>(box * 64 / 96);
  lv_obj_t* tip = lv_obj_create(root);
  lv_obj_set_size(tip, body * 3 / 4, body * 3 / 4);
  styleBare(tip);
  lv_obj_set_style_radius(tip, 6, 0);
  lv_obj_set_style_bg_color(tip, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(tip, (body * 3 / 4) / 2, 0);
  lv_obj_set_style_transform_pivot_y(tip, (body * 3 / 4) / 2, 0);
  lv_obj_set_style_transform_angle(tip, 450, 0);  // 45°
  lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, 6);

  lv_obj_t* circle = lv_obj_create(root);
  lv_obj_set_size(circle, body, body);
  styleBare(circle);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
  lv_obj_align(circle, LV_ALIGN_BOTTOM_MID, 0, -6);

  return root;
}

lv_obj_t* buildBodySquircle(lv_obj_t* parent, int16_t box, uint32_t color) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, box, box);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  styleBare(root);

  const int16_t body = static_cast<int16_t>(box * 74 / 96);
  lv_obj_t* sq = lv_obj_create(root);
  lv_obj_set_size(sq, body, body);
  lv_obj_align(sq, LV_ALIGN_CENTER, 0, 0);
  styleBare(sq);
  lv_obj_set_style_radius(sq, body * 28 / 74, 0);  // squircle
  lv_obj_set_style_bg_color(sq, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(sq, LV_OPA_COVER, 0);
  return root;
}

void setAvatarKind(AvatarKind kind) {
  g_avKind = kind;
  for (int i = 0; i < 4; ++i) {
    if (!g_avBody[i]) continue;
    if (i == static_cast<int>(kind)) lv_obj_clear_flag(g_avBody[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_avBody[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void setAvatarBadge(BadgeKind kind) {
  if (!g_avBadge) return;
  if (kind == BadgeKind::None) {
    lv_obj_add_flag(g_avBadge, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(g_avBadge, LV_OBJ_FLAG_HIDDEN);
  if (kind == BadgeKind::Check) {
    lv_obj_set_style_bg_color(g_avBadge, lv_color_hex(COL_BADGE_OK), 0);
    if (g_avBadgeLabel) lv_label_set_text(g_avBadgeLabel, LV_SYMBOL_OK);
  } else {
    lv_obj_set_style_bg_color(g_avBadge, lv_color_hex(COL_BADGE_ERR), 0);
    if (g_avBadgeLabel) lv_label_set_text(g_avBadgeLabel, LV_SYMBOL_CLOSE);
  }
}

void showAvatar(bool on, AvatarKind kind, BadgeKind badge) {
  if (!g_avatarCont) return;
  if (on) {
    lv_obj_clear_flag(g_avatarCont, LV_OBJ_FLAG_HIDDEN);
    setAvatarKind(kind);
    setAvatarBadge(badge);
  } else {
    lv_obj_add_flag(g_avatarCont, LV_OBJ_FLAG_HIDDEN);
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
    return COL_AMBER;
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
    if (g_fleetStatus[i]) {
      lv_label_set_text(g_fleetStatus[i], stats[i]);
      lv_obj_set_style_text_color(g_fleetStatus[i],
                                  lv_color_hex(statusColorForText(stats[i])), 0);
    }
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
  stopAllAnims();
  resetDiskGeometry();
  resetFleetCardBorders();
  hideAllStateConts();
  showWorkArcs(false);
  if (g_onlinePill) lv_obj_add_flag(g_onlinePill, LV_OBJ_FLAG_HIDDEN);
  if (g_fleetHint) lv_obj_add_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);
  if (g_errorRim) lv_obj_add_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);
  if (g_halo) lv_obj_add_flag(g_halo, LV_OBJ_FLAG_HIDDEN);

  if (g_titleLabel) lv_obj_clear_flag(g_titleLabel, LV_OBJ_FLAG_HIDDEN);
  if (g_messageLabel) lv_obj_clear_flag(g_messageLabel, LV_OBJ_FLAG_HIDDEN);
  if (g_disk) lv_obj_clear_flag(g_disk, LV_OBJ_FLAG_HIDDEN);

  uint32_t ring = COL_IDLE;
  enum class AnimMode : uint8_t { None, Idle, Working, Pop, Error, Fleet };
  AnimMode anim = AnimMode::None;

  switch (g_state) {
    case UiState::BOOT:
      if (g_contBoot) lv_obj_clear_flag(g_contBoot, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateBoot));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      showAvatar(true, AvatarKind::Groku, BadgeKind::None);
      ring = COL_IDLE;
      anim = AnimMode::Idle;
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
      showAvatar(true, AvatarKind::Groku, BadgeKind::None);
      ring = COL_IDLE;
      anim = AnimMode::Idle;
      break;

    case UiState::FLEET_STATUS:
      if (g_contFleet) lv_obj_clear_flag(g_contFleet, LV_OBJ_FLAG_HIDDEN);
      if (g_fleetHint) lv_obj_clear_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateFleet));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      // Small Groku center; hide ring disk (cards carry info)
      showAvatar(true, AvatarKind::Groku, BadgeKind::None);
      if (g_avatarCont) {
        // Compact Groku via zoom (children stay layout-valid)
        g_avPosX = kCx - kAvSize / 2;
        g_avPosY = kDiskCy - 56;
        lv_obj_set_pos(g_avatarCont, g_avPosX, g_avPosY);
        lv_obj_set_style_transform_pivot_x(g_avatarCont, kAvSize / 2, 0);
        lv_obj_set_style_transform_pivot_y(g_avatarCont, kAvSize / 2, 0);
        lv_obj_set_style_transform_zoom(g_avatarCont, 170, 0);  // ~66%
      }
      if (g_disk) lv_obj_add_flag(g_disk, LV_OBJ_FLAG_HIDDEN);
      if (g_titleLabel) lv_obj_add_flag(g_titleLabel, LV_OBJ_FLAG_HIDDEN);
      if (g_messageLabel) lv_obj_add_flag(g_messageLabel, LV_OBJ_FLAG_HIDDEN);
      ring = COL_LINE;
      anim = AnimMode::Fleet;
      break;

    case UiState::TOUCH_CONFIRM:
      if (g_contConfirm) lv_obj_clear_flag(g_contConfirm, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateTouchConfirm));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_MUTED), 0);
      }
      showAvatar(true, AvatarKind::Groku, BadgeKind::Check);
      ring = COL_GREEN;
      anim = AnimMode::Pop;
      break;

    case UiState::WORKING:
      if (g_contWorking) lv_obj_clear_flag(g_contWorking, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateWorking));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_AMBER), 0);
      }
      showAvatar(true, AvatarKind::EspForge, BadgeKind::None);
      showWorkArcs(true);
      ring = COL_AMBER;
      anim = AnimMode::Working;
      break;

    case UiState::DONE:
      if (g_contDone) lv_obj_clear_flag(g_contDone, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateDone));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_GREEN), 0);
      }
      showAvatar(true, AvatarKind::Groku, BadgeKind::Check);
      ring = COL_GREEN;
      anim = AnimMode::Pop;
      break;

    case UiState::ERROR:
      if (g_contError) lv_obj_clear_flag(g_contError, LV_OBJ_FLAG_HIDDEN);
      if (g_errorRim) lv_obj_clear_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);
      if (g_headerLabel) {
        lv_label_set_text(g_headerLabel, i18nStr(I18nId::StateError));
        lv_obj_set_style_text_color(g_headerLabel, lv_color_hex(COL_RED), 0);
      }
      showAvatar(true, AvatarKind::Groku, BadgeKind::Cross);
      ring = COL_RED;
      anim = AnimMode::Error;
      break;
  }

  if (g_disk) {
    lv_obj_set_style_border_color(g_disk, lv_color_hex(ring), 0);
    lv_obj_set_style_border_opa(g_disk, LV_OPA_COVER, 0);
  }

  switch (anim) {
    case AnimMode::Idle:    startIdleBreathe(); break;
    case AnimMode::Working: startWorkingAnims(); break;
    case AnimMode::Pop:     startPopScale(); break;
    case AnimMode::Error:   startErrorAnims(); break;
    case AnimMode::Fleet:   startFleetCardPulse(); break;
    case AnimMode::None:    break;
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

void buildMainAvatar(lv_obj_t* parent) {
  g_avatarCont = lv_obj_create(parent);
  lv_obj_set_size(g_avatarCont, kAvSize, kAvSize);
  lv_obj_set_pos(g_avatarCont, kAvBaseX, kAvBaseY);
  lv_obj_set_style_bg_opa(g_avatarCont, LV_OPA_TRANSP, 0);
  styleBare(g_avatarCont);
  lv_obj_add_flag(g_avatarCont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  g_avBody[static_cast<int>(AvatarKind::Groku)] =
      buildBodyGroku(g_avatarCont, kAvSize);
  g_avBody[static_cast<int>(AvatarKind::EspForge)] =
      buildBodyHex(g_avatarCont, kAvSize, COL_ESPFORGE);
  g_avBody[static_cast<int>(AvatarKind::EngDir)] =
      buildBodyTear(g_avatarCont, kAvSize, COL_ENGDIR);
  g_avBody[static_cast<int>(AvatarKind::PrintMaker)] =
      buildBodySquircle(g_avatarCont, kAvSize, COL_PRINTMAKER);

  // Shared slanted eyes on top of body
  g_avEyeL = lv_obj_create(g_avatarCont);
  lv_obj_set_size(g_avEyeL, 12, 5);
  styleBare(g_avEyeL);
  lv_obj_set_style_radius(g_avEyeL, 3, 0);
  lv_obj_set_style_bg_color(g_avEyeL, lv_color_hex(COL_EYE), 0);
  lv_obj_set_style_bg_opa(g_avEyeL, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(g_avEyeL, 6, 0);
  lv_obj_set_style_transform_pivot_y(g_avEyeL, 2, 0);
  lv_obj_set_style_transform_angle(g_avEyeL, 200, 0);
  lv_obj_align(g_avEyeL, LV_ALIGN_CENTER, -14, -10);

  g_avEyeR = lv_obj_create(g_avatarCont);
  lv_obj_set_size(g_avEyeR, 12, 5);
  styleBare(g_avEyeR);
  lv_obj_set_style_radius(g_avEyeR, 3, 0);
  lv_obj_set_style_bg_color(g_avEyeR, lv_color_hex(COL_EYE), 0);
  lv_obj_set_style_bg_opa(g_avEyeR, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(g_avEyeR, 6, 0);
  lv_obj_set_style_transform_pivot_y(g_avEyeR, 2, 0);
  lv_obj_set_style_transform_angle(g_avEyeR, 200, 0);
  lv_obj_align(g_avEyeR, LV_ALIGN_CENTER, 14, -10);

  // Status badge (check / X) bottom-right
  g_avBadge = lv_obj_create(g_avatarCont);
  lv_obj_set_size(g_avBadge, 28, 28);
  styleBare(g_avBadge);
  lv_obj_set_style_radius(g_avBadge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_avBadge, lv_color_hex(COL_BADGE_OK), 0);
  lv_obj_set_style_bg_opa(g_avBadge, LV_OPA_COVER, 0);
  lv_obj_align(g_avBadge, LV_ALIGN_BOTTOM_RIGHT, 2, 2);
  lv_obj_add_flag(g_avBadge, LV_OBJ_FLAG_HIDDEN);

  g_avBadgeLabel = lv_label_create(g_avBadge);
  lv_label_set_text(g_avBadgeLabel, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(g_avBadgeLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_avBadgeLabel, lv_color_hex(COL_BG), 0);
  lv_obj_center(g_avBadgeLabel);

  setAvatarKind(AvatarKind::Groku);
}

void buildCenterDisk(lv_obj_t* parent) {
  g_halo = lv_obj_create(parent);
  {
    const int16_t hr = kDiskR + kHaloPad;
    lv_obj_set_size(g_halo, hr * 2, hr * 2);
    lv_obj_set_pos(g_halo, kCx - hr, kDiskCy - hr);
  }
  lv_obj_set_style_radius(g_halo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_halo, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_halo, 3, 0);
  lv_obj_set_style_border_color(g_halo, lv_color_hex(COL_IDLE), 0);
  lv_obj_set_style_border_opa(g_halo, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(g_halo, 0, 0);
  lv_obj_clear_flag(g_halo, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_halo, LV_OBJ_FLAG_HIDDEN);

  // Transparent ring (border only) — avatar is the focus
  g_disk = lv_obj_create(parent);
  lv_obj_set_size(g_disk, kDiskR * 2, kDiskR * 2);
  lv_obj_set_pos(g_disk, kCx - kDiskR, kDiskCy - kDiskR);
  lv_obj_set_style_radius(g_disk, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_disk, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_disk, 4, 0);
  lv_obj_set_style_border_color(g_disk, lv_color_hex(COL_IDLE), 0);
  lv_obj_set_style_pad_all(g_disk, 0, 0);
  lv_obj_clear_flag(g_disk, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

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

  buildMainAvatar(parent);
}

lv_obj_t* buildMiniAvatar(lv_obj_t* parent, AvatarKind kind) {
  lv_obj_t* cont = lv_obj_create(parent);
  lv_obj_set_size(cont, kMiniAv, kMiniAv);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  styleBare(cont);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* body = nullptr;
  switch (kind) {
    case AvatarKind::Groku:      body = buildBodyGroku(cont, kMiniAv); break;
    case AvatarKind::EspForge:   body = buildBodyHex(cont, kMiniAv, COL_ESPFORGE); break;
    case AvatarKind::EngDir:     body = buildBodyTear(cont, kMiniAv, COL_ENGDIR); break;
    case AvatarKind::PrintMaker: body = buildBodySquircle(cont, kMiniAv, COL_PRINTMAKER); break;
  }
  (void)body;
  addSlantEyes(cont, 5, 2, 5, -4);
  return cont;
}

void buildFleetCard(lv_obj_t* parent, int idx, float angleDeg, const char* letter,
                    AvatarKind miniKind) {
  int16_t bx, by;
  const float rad = angleDeg * kPi / 180.0f;
  const float r = 155.0f;
  bx = static_cast<int16_t>(kCx + r * sinf(rad));
  by = static_cast<int16_t>(kCy + 55 + r * cosf(rad) * 0.35f);

  lv_obj_t* card = lv_btn_create(parent);
  lv_obj_set_size(card, 148, 78);
  lv_obj_set_pos(card, bx - 74, by - 39);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(COL_LINE), 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 6, 0);

  TouchZone zone = TouchZone::Z_SLOT_A;
  if (idx == 1) zone = TouchZone::Z_SLOT_B;
  if (idx == 2) zone = TouchZone::Z_SLOT_C;
  lv_obj_add_event_cb(card, onBtnClicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(zone)));

  g_fleetMini[idx] = buildMiniAvatar(card, miniKind);
  lv_obj_align(g_fleetMini[idx], LV_ALIGN_LEFT_MID, 2, 0);

  lv_obj_t* let = makeLabel(card, &lv_font_montserrat_12, lv_color_hex(COL_RED), 0);
  lv_label_set_text(let, letter);
  lv_obj_align(let, LV_ALIGN_TOP_LEFT, 40, 2);

  g_fleetName[idx] = makeLabel(card, &lv_font_montserrat_14, lv_color_hex(COL_TEXT), 96);
  lv_obj_set_style_text_align(g_fleetName[idx], LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(g_fleetName[idx], LV_ALIGN_TOP_LEFT, 40, 18);

  g_fleetDot[idx] = lv_obj_create(card);
  lv_obj_set_size(g_fleetDot[idx], 8, 8);
  lv_obj_set_style_radius(g_fleetDot[idx], LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_fleetDot[idx], lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_bg_opa(g_fleetDot[idx], LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_fleetDot[idx], 0, 0);
  lv_obj_clear_flag(g_fleetDot[idx], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(g_fleetDot[idx], LV_ALIGN_BOTTOM_LEFT, 40, -6);

  g_fleetStatus[idx] = makeLabel(card, &lv_font_montserrat_12, lv_color_hex(COL_MUTED), 80);
  lv_obj_set_style_text_align(g_fleetStatus[idx], LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(g_fleetStatus[idx], LV_ALIGN_BOTTOM_LEFT, 52, -4);

  g_fleetCard[idx] = card;
}

void buildUi() {
  g_screen = lv_scr_act();
  lv_obj_set_style_bg_color(g_screen, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rim = lv_obj_create(g_screen);
  lv_obj_set_size(rim, 472, 472);
  lv_obj_center(rim);
  lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(rim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rim, 2, 0);
  lv_obj_set_style_border_color(rim, lv_color_hex(COL_LINE), 0);
  lv_obj_clear_flag(rim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  g_errorRim = lv_obj_create(g_screen);
  lv_obj_set_size(g_errorRim, 464, 464);
  lv_obj_center(g_errorRim);
  lv_obj_set_style_radius(g_errorRim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_errorRim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_errorRim, 6, 0);
  lv_obj_set_style_border_color(g_errorRim, lv_color_hex(COL_RED), 0);
  lv_obj_clear_flag(g_errorRim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g_errorRim, LV_OBJ_FLAG_HIDDEN);

  g_headerLabel = makeLabel(g_screen, &lv_font_montserrat_12, lv_color_hex(COL_MUTED), 300);
  lv_obj_align(g_headerLabel, LV_ALIGN_TOP_MID, 0, 48);

  buildCenterDisk(g_screen);

  g_titleLabel = makeLabel(g_screen, &lv_font_montserrat_22, lv_color_hex(COL_TEXT), 360);
  lv_obj_align(g_titleLabel, LV_ALIGN_TOP_MID, 0, 300);

  g_messageLabel = makeLabel(g_screen, &lv_font_montserrat_14, lv_color_hex(COL_MUTED), 360);
  lv_obj_align(g_messageLabel, LV_ALIGN_TOP_MID, 0, 328);

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

  g_fleetHint = makeLabel(g_screen, &lv_font_montserrat_12, lv_color_hex(COL_DIM), 200);
  lv_label_set_text(g_fleetHint, i18nStr(I18nId::TouchSlotHint));
  lv_obj_align(g_fleetHint, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_add_flag(g_fleetHint, LV_OBJ_FLAG_HIDDEN);

  g_contBoot = makeStateCont(g_screen);
  g_contHome = makeStateCont(g_screen);
  g_contFleet = makeStateCont(g_screen);
  g_contConfirm = makeStateCont(g_screen);
  g_contWorking = makeStateCont(g_screen);
  g_contDone = makeStateCont(g_screen);
  g_contError = makeStateCont(g_screen);

  {
    int16_t x, y;
    arcPos(-50.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotA), false, TouchZone::Z_SLOT_A, x, y);
    arcPos(0.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotB), false, TouchZone::Z_SLOT_B, x, y);
    arcPos(50.f, &x, &y);
    makePill(g_contHome, i18nStr(I18nId::SlotC), false, TouchZone::Z_SLOT_C, x, y);
  }

  {
    lv_obj_t* flab = makeLabel(g_contFleet, &lv_font_montserrat_12, lv_color_hex(COL_DIM), 0);
    lv_label_set_text(flab, i18nStr(I18nId::FleetLabel));
    lv_obj_set_pos(flab, kCx - 20, kDiskCy + 8);

    // A PrintMaker, B EspForge, C EngDir
    buildFleetCard(g_contFleet, 0, -55.f, "A", AvatarKind::PrintMaker);
    buildFleetCard(g_contFleet, 1, 0.f, "B", AvatarKind::EspForge);
    buildFleetCard(g_contFleet, 2, 55.f, "C", AvatarKind::EngDir);
  }

  {
    int16_t x, y;
    arcPos(-40.f, &x, &y);
    makePill(g_contConfirm, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
    arcPos(40.f, &x, &y);
    makePill(g_contConfirm, i18nStr(I18nId::ActionOk), true, TouchZone::Z_ACTION_PRIMARY, x, y);
  }

  {
    int16_t x, y;
    arcPos(0.f, &x, &y);
    makePill(g_contWorking, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
  }

  {
    int16_t x, y;
    arcPos(0.f, &x, &y);
    makePill(g_contDone, i18nStr(I18nId::ActionHome), false, TouchZone::Z_ACTION_BACK, x, y);
  }

  {
    int16_t x, y;
    arcPos(-40.f, &x, &y);
    makePill(g_contError, i18nStr(I18nId::ActionBack), false, TouchZone::Z_ACTION_BACK, x, y);
    arcPos(40.f, &x, &y);
    makePill(g_contError, i18nStr(I18nId::ActionRetry), true, TouchZone::Z_ACTION_PRIMARY, x, y);
  }

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
  Serial.println(F("[display] UI v0.4 app avatars ready"));
}

void displaySetState(UiState s) {
  g_state = s;
  if (s == UiState::TOUCH_CONFIRM) {
    if (std::strcmp(g_title, i18nStr(I18nId::TitleIdle)) == 0 ||
        std::strcmp(g_title, "Groku CEO") == 0 ||
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
  } else if (s == UiState::WORKING) {
    if (std::strcmp(g_title, i18nStr(I18nId::TitleIdle)) == 0 ||
        std::strcmp(g_title, "Groku CEO") == 0) {
      std::strncpy(g_title, "EspForge", sizeof(g_title) - 1);
    }
  } else if (s == UiState::HOME) {
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

void displayFireZone(TouchZone zone) { fireZone(zone); }
