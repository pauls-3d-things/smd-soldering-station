#include <Arduino.h>
#include <AverageThermistor.h>
#include <NTC_Thermistor.h>
#include <PID_v1.h>
#include <Thermistor.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "config.h"
#include "ui.h"

Thermistor* thermistor = NULL;
float temp = 0;

#define NUM_STATES 2
enum MODE_STATE { SELECT = 0, RUNNING = 1 };
#define NUM_MODES 3
enum MODE { INFO = 0, SMD = 1, CUSTOM_TEMP = 2 };

MODE_STATE modeState = SELECT;
MODE mode = INFO;
String modeTitle = "ready";
uint16_t modeDuration = 0;
uint8_t modeProgress = 0;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// variables for output control
long heatingSince = 0;
long coolingSince = 0;
long lastChange = 0;
long runningSince = 0;

#define TEMP_WINDOW 2000  // two seconds
#define SWITCH_SPEED 25   // dont switch faster than this

// variables for PID
double targetTemp, pidInput, pidOutput;
double Kp = 2, Ki = 0.01, Kd = 0.5;
PID myPID(&pidInput, &pidOutput, &targetTemp, Kp, Ki, Kd, DIRECT);

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
  targetTemp = 40;

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

  if (heatingSince > coolingSince && pidOutput < 255) {  // TODO: remove the < ?
    // we are (partially) heating

    if (now - heatingSince > TEMP_WINDOW * (pidOutput / 255.0)) {
      // turn off
      digitalWrite(PIN_SSR, LOW);
      coolingSince = now;
      lastChange = now;
    }
  } else if (pidOutput > 4) {  // magic value to limit overshooting a bit
    // we are (partially) cooling
    if (now - coolingSince > TEMP_WINDOW * ((255 - pidOutput) / 255.0)) {
      // turn on
      digitalWrite(PIN_SSR, HIGH);
      heatingSince = now;
      lastChange = now;
    }
  }
}
long counter = 0;
void drawUI(U8G2 u8g2) {
  counter++;

  // left: status
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x8_tf);
  button(u8g2, 0, 50, 42, modeState == RUNNING ? "Stop" : "Start", modeState == RUNNING);
  dialog(u8g2, 0, 0, 42, 49, String("Status"));
  if (temp >= 40) {
    u8g2.setDrawColor(1);
    u8g2.drawTriangle(6, 38, 21, 13, 35, 38);

    u8g2.setDrawColor((counter + 1) % 2);
    u8g2.drawTriangle(9, 36, 21, 16, 32, 36);

    u8g2.setDrawColor(counter % 2);
    u8g2.setFont(u8g2_font_profont22_tf);
    u8g2.drawStr(16, 35, "!");

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.drawStr(11, 47, (String(temp, 0) + "C").c_str());
  } else {
    u8g2.setFont(u8g2_font_profont22_tf);
    u8g2.drawStr(16, 35, "*");

    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.setDrawColor(1);
    u8g2.drawStr(11, 47, "<40C");
  }

  // right: mode
  u8g2.setFont(u8g2_font_5x8_tf);
  if (modeState != RUNNING || mode == CUSTOM_TEMP) {
    button(u8g2, 43, 50, 42, "< ", false);
    button(u8g2, 86, 50, 42, ">", false);
  }

  switch (mode) {
    case INFO:
      dialog(u8g2, 43, 0, 85, 49, String("Info"));
      u8g2.drawStr(49, 18, "SMD Solderplate");
      u8g2.drawStr(70, 27, "v1.0.0");
      u8g2.drawStr(66, 36, "p3dt.net");
      u8g2.drawStr(46, 45, "@pauls_3d_things");
      break;
    case CUSTOM_TEMP:
      dialog(u8g2, 43, 0, 85, 49, String("Custom Temp."));
      u8g2.setFont(u8g2_font_profont22_tf);
      u8g2.drawStr(64, 35, (String(targetTemp, 0) + "C").c_str());
      break;
    case SMD:
      dialog(u8g2, 43, 0, 85, 49, String(modeState == SELECT ? "SMD Soldering" : modeTitle));

      u8g2.drawStr(46, 18, (String(targetTemp, 0) + "C").c_str());
      u8g2.drawStr(100, 18, (String(modeDuration) + "s").c_str());
      if (modeState == RUNNING) {
        progressBar(u8g2, 43, 50, 85, 14, modeProgress);
      }
      u8g2.drawFrame(44, 20, 83, 28);
      break;
  }
}

void loop(void) {
  temp = thermistor->readCelsius();
  if (modeState == RUNNING) {
    pidInput = temp;
    myPID.Compute();
    applyOutput();

    switch (mode) {
      case INFO:
      case CUSTOM_TEMP:
        // nothing to do here!
        break;
      case SMD:
        long elapsed = (millis() - runningSince) / 1000;
        if (elapsed < 20) {
          targetTemp = 80;
          modeTitle = "pre-heat";
          modeDuration = 20;
        } else if (elapsed < 60) {
          targetTemp = 150;
          modeTitle = "pre-heat";
          modeDuration = 40;
        } else if (elapsed < 120) {
          targetTemp = 180;
          modeTitle = "soak";
          modeDuration = 60;
        } else if (elapsed < 160) {
          targetTemp = 230;
          modeTitle = "reflow";
          modeDuration = 40;
        } else {
          targetTemp = 0;
          modeTitle = "cooldown";
          modeDuration = 0;
          modeState = SELECT;
        }
        modeProgress = (elapsed / 160.0) * 100;
        break;
    }
  } else {
    digitalWrite(PIN_SSR, LOW);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  drawUI(u8g2);
  u8g2.sendBuffer();

  // LOW = button down
  if (digitalRead(BTN_LEFT) == LOW) {
    switch (modeState) {
      case SELECT:
        modeState = RUNNING;
        runningSince = millis();
        break;
      case RUNNING:
        modeState = SELECT;
        break;
    }
    delay(500);
  }
  if (digitalRead(BTN_MID) == LOW) {
    if (modeState == SELECT) {
      mode = (MODE)((mode - 1) % NUM_MODES);
    } else if (mode == CUSTOM_TEMP) {
      targetTemp--;
    }
    delay(100);
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    if (modeState == SELECT) {
      mode = (MODE)((mode + 1) % NUM_MODES);
    } else if (mode == CUSTOM_TEMP) {
      targetTemp++;
    }
    delay(100);
  }

  delay(50);
}
