#pragma once
// Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B (SKU 30697) — HW-LEAD-PACK / wiki
// Official RGB ST7701 + CST820 + TCA9554 map. Do not invent GPIOs.

// --- Backlight / battery / boot ---
#define PIN_LCD_BL     6
#define PIN_BAT_ADC    4
#define PIN_BOOT       0

// --- LCD init SPI (ST7701 command bus) ---
#define PIN_LCD_SDA    1   // MOSI
#define PIN_LCD_SCL    2   // SCLK
// RST = EXIO1, CS = EXIO3 (via TCA9554)

// --- LCD RGB parallel ---
#define PIN_LCD_PCLK   41
#define PIN_LCD_DE     40
#define PIN_LCD_VSYNC  39
#define PIN_LCD_HSYNC  38

// Blue (B0 NC on panel; B1–B5 wired)
#define PIN_LCD_B1     5
#define PIN_LCD_B2     45
#define PIN_LCD_B3     48
#define PIN_LCD_B4     47
#define PIN_LCD_B5     21

// Green G0–G5
#define PIN_LCD_G0     14
#define PIN_LCD_G1     13
#define PIN_LCD_G2     12
#define PIN_LCD_G3     11
#define PIN_LCD_G4     10
#define PIN_LCD_G5     9

// Red (R0 NC; R1–R5 wired)
#define PIN_LCD_R1     46
#define PIN_LCD_R2     3
#define PIN_LCD_R3     8
#define PIN_LCD_R4     18
#define PIN_LCD_R5     17

// --- Touch CST820 (shared onboard I2C) ---
#define PIN_I2C_SDA    15
#define PIN_I2C_SCL    7
#define PIN_TP_INT     16
// RST = EXIO2
#define CST820_ADDR    0x15

// --- TCA9554PWR I2C GPIO expander ---
#define TCA9554_ADDR   0x20
#define EXIO_LCD_RST   1   // EXIO1
#define EXIO_TP_RST    2   // EXIO2
#define EXIO_LCD_CS    3   // EXIO3
#define EXIO_TF_CS     4   // EXIO4
#define EXIO_IMU_INT2  5   // EXIO5
#define EXIO_IMU_INT1  6   // EXIO6
#define EXIO_RTC_INT   7   // EXIO7
#define EXIO_BUZZER    8   // EXIO8
