#include <Arduino.h>
#include <Wire.h>
#include <U8x8lib.h>
#include <DHT20.h>
#include "Seeed_BMP280.h"
#include <RTClib.h>

DHT20 dht20;
BMP280 bmp280;
RTC_DS1307 rtc;

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

const int buttonPin = 6;
const int potPin = A0;
const int buzzerPin = 5;

int selected = 0;
bool inMeasure = false;

bool lastButton = LOW;
unsigned long lastButtonTime = 0;
unsigned long buttonDelay = 200;

unsigned long lastBeepTime = 0;
const unsigned long beepCooldown = 3000;

void beepError() {
  if (millis() - lastBeepTime < beepCooldown) return;
  lastBeepTime = millis();
  tone(buzzerPin, 2000);
  delay(2000);
  noTone(buzzerPin);
}

void printPaddedFloat(uint8_t col, uint8_t row, float val, uint8_t decimals, uint8_t width) {
  u8x8.setCursor(col, row);
  for (uint8_t i = 0; i < width; i++) u8x8.print(' ');
  u8x8.setCursor(col, row);
  u8x8.print(val, decimals);
}

void clearLineFrom(uint8_t row, uint8_t startCol) {
  u8x8.setCursor(startCol, row);
  for (uint8_t i = startCol; i < 16; i++) u8x8.print(' ');
}

bool rtcPresent() {
  Wire.beginTransmission(0x68);
  return (Wire.endTransmission() == 0);
}

void drawMenu() {
  u8x8.clearDisplay();
  u8x8.setCursor(0, 1);
  u8x8.print(selected == 0 ? "> POMIAR KLIMATU" : "  POMIAR KLIMATU");
  u8x8.setCursor(0, 3);
  u8x8.print(selected == 1 ? "> CISNIENIE" : "  CISNIENIE");
  u8x8.setCursor(0, 5);
  u8x8.print(selected == 2 ? "> ZEGAR (RTC)" : "  ZEGAR (RTC)");
}

void drawClimateLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(1, 0);
  u8x8.print("POMIAR KLIMATU");
  u8x8.setCursor(0, 2);
  u8x8.print("Temp:");
  u8x8.setCursor(0, 4);
  u8x8.print("Wilg:");
  u8x8.setCursor(14, 2);
  u8x8.print("C");
  u8x8.setCursor(14, 4);
  u8x8.print("%");
  u8x8.setCursor(0, 6);
  u8x8.print("Status: ----   ");
}

void drawPressureLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(3, 0);
  u8x8.print("CISNIENIE");
  u8x8.setCursor(0, 2);
  u8x8.print("hPa:");
  u8x8.setCursor(0, 4);
  u8x8.print("Status: ----   ");
}

void drawRtcLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(5, 0);
  u8x8.print("ZEGAR");
  u8x8.setCursor(0, 2);
  u8x8.print("Godz:");
  u8x8.setCursor(0, 4);
  u8x8.print("Data:");
  u8x8.setCursor(0, 6);
  u8x8.print("Status: ----   ");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  dht20.begin();
  bmp280.init();
  rtc.begin();
  pinMode(buttonPin, INPUT);
  pinMode(potPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  noTone(buzzerPin);
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  drawMenu();
}

void loop() {
  bool errorNow = false;

  bool button = digitalRead(buttonPin);

  if (button == HIGH && lastButton == LOW && millis() - lastButtonTime > buttonDelay) {
    lastButtonTime = millis();
    inMeasure = !inMeasure;
    if (inMeasure) {
      if (selected == 0) drawClimateLayout();
      else if (selected == 1) drawPressureLayout();
      else drawRtcLayout();
    } else {
      drawMenu();
    }
  }

  lastButton = button;

  if (!inMeasure) {
    int pot = analogRead(potPin);
    int newSelected = 0;
    if (pot > 682) newSelected = 2;
    else if (pot > 341) newSelected = 1;
    else newSelected = 0;
    if (newSelected != selected) {
      selected = newSelected;
      drawMenu();
    }
    delay(50);
    return;
  }

  if (selected == 0) {
    int status = dht20.read();
    if (status == 0) {
      printPaddedFloat(6, 2, dht20.getTemperature(), 1, 7);
      printPaddedFloat(6, 4, dht20.getHumidity(), 1, 7);
      u8x8.setCursor(0, 6);
      u8x8.print("Status: OK     ");
    } else {
      u8x8.setCursor(0, 6);
      u8x8.print("Status: BLAD   ");
      errorNow = true;
    }
  }
  else if (selected == 1) {
    float pressure = bmp280.getPressure() / 100.0;
    if (pressure > 0) {
      printPaddedFloat(4, 2, pressure, 0, 9);
      u8x8.setCursor(0, 4);
      u8x8.print("Status: OK     ");
    } else {
      clearLineFrom(2, 4);
      u8x8.setCursor(4, 2);
      u8x8.print("BLAD");
      u8x8.setCursor(0, 4);
      u8x8.print("Status: BLAD   ");
      errorNow = true;
    }
  }
  else {
    const uint8_t valueCol = 6;
    if (!rtcPresent()) {
      u8x8.setCursor(0, 6);
      u8x8.print("Status: BLAD   ");
      clearLineFrom(2, valueCol);
      u8x8.setCursor(valueCol, 2);
      u8x8.print("--:--:--");
      clearLineFrom(4, valueCol);
      u8x8.setCursor(valueCol, 4);
      u8x8.print("--.--.----");
      errorNow = true;
    }
    else {
      if (!rtc.isrunning()) {
        u8x8.setCursor(0, 6);
        u8x8.print("Status: USTAW  ");
      } else {
        u8x8.setCursor(0, 6);
        u8x8.print("Status: OK     ");
      }

      DateTime now = rtc.now();
      char tbuf[17];
      char dbuf[17];
      snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      snprintf(dbuf, sizeof(dbuf), "%02d.%02d.%04d", now.day(), now.month(), now.year());
      clearLineFrom(2, valueCol);
      u8x8.setCursor(valueCol, 2);
      u8x8.print(tbuf);
      clearLineFrom(4, valueCol);
      u8x8.setCursor(valueCol, 4);
      u8x8.print(dbuf);
    }
  }

  if (errorNow) beepError();

  delay(1000);
}