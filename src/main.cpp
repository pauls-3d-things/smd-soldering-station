#include <Arduino.h>
#include <AverageThermistor.h>
#include <NTC_Thermistor.h>
#include <Thermistor.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "config.h"

Thermistor* thermistor = NULL;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

boolean ssrOn = false;

void setupButtons() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MID, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
}

void setupThermistor() {
  Thermistor* originThermistor =
      new NTC_Thermistor(SENSOR_PIN, REFERENCE_RESISTANCE, NOMINAL_RESISTANCE, NOMINAL_TEMPERATURE, B_VALUE);
  thermistor = new AverageThermistor(originThermistor, READINGS_NUMBER, DELAY_TIME);
}

void setupSSR() {
    pinMode(PIN_SSR, OUTPUT);
}

void setup(void) {
  Wire.begin(5, 4);  // (CLK,SDA)
  u8g2.begin();
  setupButtons();

  setupThermistor();
  setupSSR();
}

void loop(void) {
  digitalWrite(PIN_SSR, ssrOn ? HIGH : LOW);

  float temp = thermistor->readCelsius();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 15, (String("Temp (C): ") + String(temp, 1)).c_str());
  u8g2.drawStr(0, 25, ssrOn ? "SSR: ON" : "SSR: OFF");
  u8g2.drawStr(0, 35, (String("A0: ") + String(analogRead(A0))).c_str());
  u8g2.sendBuffer();

  if (digitalRead(BTN_RIGHT) == LOW) {  // LOW = button down
    ssrOn = !ssrOn;
  }
  delay(50);
}
