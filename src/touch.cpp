#include "touch.h"
#include "pins.h"
#include "tca9554.h"
#include "display.h"
#include "config.h"

#include <Wire.h>
#include <lvgl.h>
#include <cmath>

namespace {

TouchZoneCallback g_cb = nullptr;
lv_indev_drv_t g_indevDrv;
bool g_pressed = false;
int16_t g_lastX = 0;
int16_t g_lastY = 0;
uint32_t g_pressStartMs = 0;
bool g_wifiHoldFired = false;

constexpr uint8_t kRegFinger = 0x02;
constexpr uint32_t kWifiHoldMs = 800;

bool cst820Read(int16_t* x, int16_t* y, bool* pressed) {
  Wire.beginTransmission(CST820_ADDR);
  Wire.write(kRegFinger);
  if (Wire.endTransmission(false) != 0) {
    *pressed = false;
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(CST820_ADDR), 6) < 6) {
    *pressed = false;
    return false;
  }
  const uint8_t fingers = static_cast<uint8_t>(Wire.read());
  const uint8_t xh = static_cast<uint8_t>(Wire.read());
  const uint8_t xl = static_cast<uint8_t>(Wire.read());
  const uint8_t yh = static_cast<uint8_t>(Wire.read());
  const uint8_t yl = static_cast<uint8_t>(Wire.read());
  Wire.read();  // discard
  *pressed = (fingers > 0) && (fingers < 3);
  *x = static_cast<int16_t>(((xh & 0x0F) << 8) | xl);
  *y = static_cast<int16_t>(((yh & 0x0F) << 8) | yl);
  return true;
}

void indevRead(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  data->point.x = g_lastX;
  data->point.y = g_lastY;
  data->state = g_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void defaultZoneCb(TouchZone zone) { displayFireZone(zone); }

}  // namespace

TouchZone touchHitTest(int16_t x, int16_t y) {
  const int16_t cx = 240;
  const int16_t cy = 240;
  const int16_t dx = static_cast<int16_t>(x - cx);
  const int16_t dy = static_cast<int16_t>(y - cy);
  const int32_t r2 = static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy;
  if (r2 > (230L * 230L)) {
    return TouchZone::None;
  }

  // Top band: Wi‑Fi hold zone
  if (y < 90) {
    return TouchZone::Z_WIFI;
  }

  const UiState st = displayGetState();

  switch (st) {
    case UiState::HOME:
      // Lower arc pills A / B / C
      if (y > 340) {
        if (x < 160) return TouchZone::Z_SLOT_A;
        if (x > 320) return TouchZone::Z_SLOT_C;
        return TouchZone::Z_SLOT_B;
      }
      break;

    case UiState::FLEET_STATUS:
      // Three arc cards roughly y 280–380
      if (y > 250 && y < 400) {
        if (x < 160) return TouchZone::Z_SLOT_A;
        if (x > 320) return TouchZone::Z_SLOT_C;
        return TouchZone::Z_SLOT_B;
      }
      break;

    case UiState::TOUCH_CONFIRM:
    case UiState::ERROR:
      if (y > 360) {
        return (x < cx) ? TouchZone::Z_ACTION_BACK : TouchZone::Z_ACTION_PRIMARY;
      }
      break;

    case UiState::WORKING:
    case UiState::DONE:
      if (y > 360) {
        return TouchZone::Z_ACTION_BACK;
      }
      break;

    default:
      if (y > 390) {
        return (x < cx) ? TouchZone::Z_ACTION_BACK : TouchZone::Z_ACTION_PRIMARY;
      }
      break;
  }

  (void)cy;
  return TouchZone::None;
}

void touchInit() {
  tca9554SetDirection(EXIO_TP_RST, false);
  tca9554SetPin(EXIO_TP_RST, false);
  delay(10);
  tca9554SetPin(EXIO_TP_RST, true);
  delay(50);
  pinMode(PIN_TP_INT, INPUT_PULLUP);

  lv_indev_drv_init(&g_indevDrv);
  g_indevDrv.type = LV_INDEV_TYPE_POINTER;
  g_indevDrv.read_cb = indevRead;
  lv_indev_drv_register(&g_indevDrv);

  g_cb = defaultZoneCb;
  Serial.println(F("[touch] CST820 + LVGL indev + zones v0.2"));
}

void touchSetCallback(TouchZoneCallback cb) { g_cb = cb; }

void touchPoll() {
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
  cst820Read(&x, &y, &pressed);

  static bool wasPressed = false;
  g_pressed = pressed;
  if (pressed) {
    g_lastX = x;
    g_lastY = y;
  }

  if (pressed && !wasPressed) {
    g_pressStartMs = millis();
    g_wifiHoldFired = false;
    const TouchZone z = touchHitTest(x, y);
    // Immediate fire for non-WiFi; WiFi requires hold
    if (z != TouchZone::None && z != TouchZone::Z_WIFI && g_cb) {
      Serial.print(F("[touch] zone="));
      Serial.println(static_cast<int>(z));
      g_cb(z);
    }
  }

  if (pressed && wasPressed && !g_wifiHoldFired) {
    const TouchZone z = touchHitTest(g_lastX, g_lastY);
    if (z == TouchZone::Z_WIFI && (millis() - g_pressStartMs) >= kWifiHoldMs) {
      g_wifiHoldFired = true;
      Serial.println(F("[touch] zone=WIFI hold"));
      if (g_cb) g_cb(TouchZone::Z_WIFI);
    }
  }

  wasPressed = pressed;
}
