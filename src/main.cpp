// Scaffold stub — round LCD 2.1B. EspForge fills LVGL/RGB/touch.
#include <Arduino.h>
#include "pins.h"
#include "config.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("grokbot-round-lcd-21 scaffold — HOME stub");
  Serial.println("SKU 30697 2.1B · Arduino-PIO+LVGL · no HA/Awtrix");
}

void loop() {
  delay(1000);
}
