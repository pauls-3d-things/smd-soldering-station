#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define BTN_LEFT 12
#define BTN_MID 14
#define BTN_RIGHT 13

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setupButtons() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MID, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
}

void setup(void) {
  Wire.begin(5, 4); // (CLK,SDA)
  u8g2.begin();
  setupButtons();
}

void loop(void) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 25, digitalRead(BTN_LEFT) ? "BTN_LEFT: UP" : "BTN_LEFT: DOWN");
  u8g2.drawStr(0, 35, digitalRead(BTN_MID) ? "BTN_MID: UP" : "BTN_MID: DOWN");
  u8g2.drawStr(0, 45, digitalRead(BTN_RIGHT) ? "BTN_RIGHT: UP" : "BTN_RIGHT: DOWN");
  u8g2.sendBuffer();

  delay(50);
}
