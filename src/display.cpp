#include "display.h"
#include "pins.h"
#include "config.h"
#include "tca9554.h"
#include "i18n.h"

#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <cstring>
#include <cctype>
#include <esp_heap_caps.h>

namespace {

Arduino_DataBus* g_bus = nullptr;
Arduino_ESP32RGBPanel* g_rgbpanel = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;

UiState g_state = UiState::BOOT;
UiState g_lastLogged = static_cast<UiState>(0xFF);

char g_title[40] = "Grok Bot";
char g_message[80] = "idle";
char g_slotA[32] = "A: —";
char g_slotB[32] = "B: —";
char g_slotC[32] = "C: —";

lv_obj_t* g_titleLabel = nullptr;
lv_obj_t* g_messageLabel = nullptr;
lv_obj_t* g_slotALabel = nullptr;
lv_obj_t* g_slotBLabel = nullptr;
lv_obj_t* g_slotCLabel = nullptr;
lv_obj_t* g_stateLabel = nullptr;
lv_obj_t* g_ring = nullptr;

lv_disp_draw_buf_t g_drawBuf;
lv_disp_drv_t g_dispDrv;
lv_color_t* g_colorBuf = nullptr;
constexpr int kBufLines = 40;

void panelHwReset() {
  tca9554SetDirection(EXIO_LCD_RST, false);
  tca9554SetDirection(EXIO_LCD_CS, false);
  tca9554SetPin(EXIO_LCD_CS, true);   // deselect
  tca9554SetPin(EXIO_LCD_RST, false); // assert reset
  delay(20);
  tca9554SetPin(EXIO_LCD_RST, true);  // release
  delay(50);
  tca9554SetPin(EXIO_LCD_CS, false);  // select for SPI init
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

void applyLabels() {
  if (g_titleLabel) lv_label_set_text(g_titleLabel, g_title);
  if (g_messageLabel) lv_label_set_text(g_messageLabel, g_message);
  if (g_slotALabel) lv_label_set_text(g_slotALabel, g_slotA);
  if (g_slotBLabel) lv_label_set_text(g_slotBLabel, g_slotB);
  if (g_slotCLabel) lv_label_set_text(g_slotCLabel, g_slotC);
  if (g_stateLabel) lv_label_set_text(g_stateLabel, uiStateName(g_state));

  if (g_ring) {
    lv_color_t c = lv_color_hex(0x3A7BD5);
    switch (g_state) {
      case UiState::WORKING: c = lv_color_hex(0xE8A838); break;
      case UiState::DONE:    c = lv_color_hex(0x3DDC97); break;
      case UiState::ERROR:   c = lv_color_hex(0xE74C3C); break;
      case UiState::TOUCH_CONFIRM: c = lv_color_hex(0x9B59B6); break;
      default: break;
    }
    lv_obj_set_style_border_color(g_ring, c, 0);
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color,
                    lv_coord_t width) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  return label;
}

bool gfxBegin() {
  // Soft-SPI for ST7701 init; CS/RST via TCA9554 EXIO
  g_bus = new Arduino_SWSPI(
      GFX_NOT_DEFINED /* DC */, GFX_NOT_DEFINED /* CS */,
      PIN_LCD_SCL /* SCK */, PIN_LCD_SDA /* MOSI */, GFX_NOT_DEFINED /* MISO */);

  // Arduino_ESP32RGBPanel: DE,VSYNC,HSYNC,PCLK, R0-4, G0-5, B0-4
  // Map panel R1→R0 slot, B1→B0 slot (R0/B0 NC on Waveshare 2.1)
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
  tca9554SetPin(EXIO_LCD_CS, true);  // deselect after init
  backlightOn();
  return ok;
}

void buildHomeUi() {
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B0F14), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Circular ring framing the 480 round panel
  g_ring = lv_obj_create(screen);
  lv_obj_set_size(g_ring, 460, 460);
  lv_obj_center(g_ring);
  lv_obj_set_style_radius(g_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ring, 4, 0);
  lv_obj_set_style_border_color(g_ring, lv_color_hex(0x3A7BD5), 0);
  lv_obj_set_style_pad_all(g_ring, 0, 0);
  lv_obj_clear_flag(g_ring, LV_OBJ_FLAG_SCROLLABLE);

  const lv_color_t cream = lv_color_hex(0xF4F1EA);
  const lv_color_t amber = lv_color_hex(0xE8A838);
  const lv_color_t mute = lv_color_hex(0x9AA4B2);
  const lv_color_t slot = lv_color_hex(0xC8D0DC);

  std::strncpy(g_title, i18nStr(I18nId::TitleDefault), sizeof(g_title) - 1);
  std::strncpy(g_message, i18nStr(I18nId::MsgIdle), sizeof(g_message) - 1);

  g_titleLabel = makeLabel(screen, &lv_font_montserrat_20, cream, 360);
  lv_obj_align(g_titleLabel, LV_ALIGN_TOP_MID, 0, 70);

  g_messageLabel = makeLabel(screen, &lv_font_montserrat_16, amber, 360);
  lv_obj_align(g_messageLabel, LV_ALIGN_TOP_MID, 0, 110);

  g_slotALabel = makeLabel(screen, &lv_font_montserrat_14, slot, 300);
  lv_obj_align(g_slotALabel, LV_ALIGN_CENTER, 0, -30);
  g_slotBLabel = makeLabel(screen, &lv_font_montserrat_14, slot, 300);
  lv_obj_align(g_slotBLabel, LV_ALIGN_CENTER, 0, 0);
  g_slotCLabel = makeLabel(screen, &lv_font_montserrat_14, slot, 300);
  lv_obj_align(g_slotCLabel, LV_ALIGN_CENTER, 0, 30);

  g_stateLabel = makeLabel(screen, &lv_font_montserrat_14, mute, 300);
  lv_obj_align(g_stateLabel, LV_ALIGN_BOTTOM_MID, 0, -70);

  applyLabels();
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

  buildHomeUi();
  displaySetState(UiState::HOME);
  Serial.println(F("[display] HOME circular 480x480 ready"));
}

void displaySetState(UiState s) {
  g_state = s;
  applyLabels();
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
  if (slot == 'A' || slot == 'a') {
    dest = g_slotA;
    n = sizeof(g_slotA);
  } else if (slot == 'B' || slot == 'b') {
    dest = g_slotB;
    n = sizeof(g_slotB);
  } else if (slot == 'C' || slot == 'c') {
    dest = g_slotC;
    n = sizeof(g_slotC);
  } else {
    return;
  }
  std::snprintf(dest, n, "%c: %s", static_cast<char>(toupper(slot)), text);
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
