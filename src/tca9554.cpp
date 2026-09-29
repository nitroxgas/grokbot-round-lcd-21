#include "tca9554.h"
#include "pins.h"
#include <Wire.h>

namespace {
constexpr uint8_t kRegInput = 0x00;
constexpr uint8_t kRegOutput = 0x01;
constexpr uint8_t kRegPolarity = 0x02;
constexpr uint8_t kRegConfig = 0x03;

uint8_t g_output = 0xFF;
uint8_t g_config = 0x00;  // 0 = output

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t* out) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(TCA9554_ADDR), 1) != 1) {
    return false;
  }
  *out = static_cast<uint8_t>(Wire.read());
  return true;
}

bool validExio(uint8_t exio) {
  return exio >= 1 && exio <= 8;
}

uint8_t bitOf(uint8_t exio) {
  return static_cast<uint8_t>(1u << (exio - 1));
}
}  // namespace

bool tca9554Init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  delay(20);
  // All pins output, initially high (inactive for CS/RST active-low, buzzer quiet)
  g_config = 0x00;
  g_output = 0xFF;
  if (!writeReg(kRegConfig, g_config)) {
    Serial.println(F("[tca9554] config write fail"));
    return false;
  }
  if (!writeReg(kRegOutput, g_output)) {
    Serial.println(F("[tca9554] output write fail"));
    return false;
  }
  writeReg(kRegPolarity, 0x00);
  Serial.println(F("[tca9554] ok @0x20"));
  return true;
}

bool tca9554SetDirection(uint8_t exio, bool input) {
  if (!validExio(exio)) {
    return false;
  }
  const uint8_t m = bitOf(exio);
  if (input) {
    g_config |= m;
  } else {
    g_config = static_cast<uint8_t>(g_config & ~m);
  }
  return writeReg(kRegConfig, g_config);
}

bool tca9554SetPin(uint8_t exio, bool high) {
  if (!validExio(exio)) {
    return false;
  }
  const uint8_t m = bitOf(exio);
  if (high) {
    g_output |= m;
  } else {
    g_output = static_cast<uint8_t>(g_output & ~m);
  }
  return writeReg(kRegOutput, g_output);
}

bool tca9554GetPin(uint8_t exio, bool* highOut) {
  if (!validExio(exio) || highOut == nullptr) {
    return false;
  }
  uint8_t v = 0;
  if (!readReg(kRegInput, &v)) {
    return false;
  }
  *highOut = (v & bitOf(exio)) != 0;
  return true;
}
