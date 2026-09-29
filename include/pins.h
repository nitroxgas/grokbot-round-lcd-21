#pragma once
// Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B — from HW-LEAD-PACK (official wiki)
#define PIN_LCD_BL 6
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 7
#define PIN_TP_INT 16
#define PIN_BAT_ADC 4
#define PIN_BOOT 0
// Buzzer via TCA9554 EXIO8 (I2C 0x20) — not a direct GPIO
#define TCA9554_ADDR 0x20
#define EXIO_BUZZER 8
