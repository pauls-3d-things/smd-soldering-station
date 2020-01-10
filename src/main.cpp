#include <Arduino.h>
#include <AverageThermistor.h>
#include <NTC_Thermistor.h>
#include <PID_v1.h>
#include <Thermistor.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "config.h"

Thermistor* thermistor = NULL;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// variables for output control
boolean ssrOn = false;
long heatingSince = 0;
long coolingSince = 0;
long lastChange = 0;

#define TEMP_WINDOW 2000  // two seconds
#define SWITCH_SPEED 25  // dont switch faster than this

// variables for PID
double Setpoint, Input, Output;
double Kp = 2, Ki = 0.01, Kd = 0.5;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

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
  coolingSince = millis();
  Setpoint = 100;

  // turn the PID on
  myPID.SetMode(AUTOMATIC);
}

void setup(void) {
  Serial.begin(115200);
  Wire.begin(5, 4);  // (CLK,SDA)
  u8g2.begin();
  setupButtons();

  setupThermistor();
  setupSSR();
}

void applyOutput() {
  long now = millis();
  if (now - lastChange < SWITCH_SPEED) {
    // do not switch faster than SWITCH_SPEED
    return;
  }

  // approach: never heat longer than
  // window * (Output/maxOutput)
  // => 255 => always on
  // => 0 => always off

  if (heatingSince > coolingSince && Output < 255) {
    // we are (partially) heating

    if (now - heatingSince > TEMP_WINDOW * (Output / 255.0)) {
      // turn off
      digitalWrite(PIN_SSR, LOW);
      coolingSince = now;
      lastChange = now;
    }
  } else if (Output > 0) {
    // we are (partially) cooling
    if (now - coolingSince > TEMP_WINDOW * ((255 - Output) / 255.0)) {
      // turn on
      digitalWrite(PIN_SSR, HIGH);
      heatingSince = now;
      lastChange = now;
    }
  }
}

void loop(void) {
  float temp = thermistor->readCelsius();
  if (ssrOn) {
    Input = temp;
    myPID.Compute();
    applyOutput();
  } else {
    digitalWrite(PIN_SSR, LOW);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 15, (String("Temp (C): ") + String(temp, 1)).c_str());
  u8g2.drawStr(0, 25, ssrOn ? "SSR: ON" : "SSR: OFF");
  u8g2.drawStr(64, 25, heatingSince > coolingSince ? "HEAT: ON" : "HEAT: OFF");
  u8g2.drawStr(0, 35, (String("Input: ") + String(Input)).c_str());
  u8g2.drawStr(0, 45, (String("Setpoint: ") + String(Setpoint)).c_str());
  u8g2.drawStr(0, 55, (String("Output: ") + String(Output)).c_str());
  u8g2.sendBuffer();

  // LOW = button down
  if (digitalRead(BTN_LEFT) == LOW) {
    ssrOn = !ssrOn;
    delay(500);
  }
  if (digitalRead(BTN_MID) == LOW) {
    Setpoint--;
    delay(100);
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    Setpoint++;
    delay(100);
  }

  delay(50);
}
