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
const int lightPin = A6;
const int ledPin = 3;


const int ledSensePin = 4;


unsigned long lastLedCheck = 0;
const unsigned long ledCheckInterval = 1000;
bool ledCableOkCached = true;

int selected = 0;
bool inMeasure = false;

bool lastButton = LOW;
unsigned long lastButtonTime = 0;
unsigned long buttonDelay = 200;

unsigned long lastBeepTime = 0;
const unsigned long beepCooldown = 3000;

bool beeping = false;
unsigned long beepStart = 0;
const unsigned long beepDuration = 2000;

uint8_t currentContrast = 120;

bool brightnessMoved = false;
int lastPotRaw = 0;

int LIGHT_RAW_MIN = 0;
int LIGHT_RAW_MAX = 775;

unsigned long lightBadSince = 0;

void startBeep() {
  if (millis() - lastBeepTime < beepCooldown) return;
  lastBeepTime = millis();
  beeping = true;
  beepStart = millis();
  tone(buzzerPin, 2000);
}

void updateBeep() {
  if (!beeping) return;
  if (millis() - beepStart >= beepDuration) {
    beeping = false;
    noTone(buzzerPin);
  }
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

const char* menuItems[] = {
  "POMIAR KLIMATU",
  "CISNIENIE",
  "ZEGAR (RTC)",
  "JASNOSC OLED",
  "SWIATLO"
};
const int menuCount = 5;

void drawMenu() {
  u8x8.clearDisplay();

  int startIdx = 0;
  if (selected > 3) startIdx = selected - 3;

  for (int i = 0; i < 4 && (startIdx + i) < menuCount; i++) {
    int idx = startIdx + i;
    u8x8.setCursor(0, 1 + i * 2);
    if (idx == selected) u8x8.print("> ");
    else u8x8.print("  ");
    u8x8.print(menuItems[idx]);
  }
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

void drawBrightnessLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(2, 0);
  u8x8.print("JASNOSC OLED");
  u8x8.setCursor(0, 2);
  u8x8.print("Poziom:");
  u8x8.setCursor(0, 6);
  u8x8.print("Status: OK     ");
  brightnessMoved = false;
  lastPotRaw = analogRead(potPin);
}

void drawLightLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(3, 0);
  u8x8.print("SWIATLO");
  u8x8.setCursor(0, 2);
  u8x8.print("Wartosc:");
  u8x8.setCursor(0, 4);
  u8x8.print("Poziom:");
  u8x8.setCursor(0, 7);
  u8x8.print("Status: ----   ");
}

void drawBrightnessBar(uint8_t percent) {
  const uint8_t barLen = 12;
  uint8_t filled = (uint16_t)percent * barLen / 100;

  u8x8.setCursor(0, 4);
  u8x8.print("      ");
  u8x8.setCursor(0, 4);
  if (percent < 100) u8x8.print(' ');
  if (percent < 10) u8x8.print(' ');
  u8x8.print((int)percent);
  u8x8.print("%");

  u8x8.setCursor(0, 5);
  u8x8.print("[");
  for (uint8_t i = 0; i < barLen; i++) u8x8.print(i < filled ? '#' : '-');
  u8x8.print("]");
}

uint8_t currentPercentFromContrast() {
  uint16_t y = (uint16_t)currentContrast * 255;
  uint8_t x = (uint8_t)sqrt((double)y);
  return map(x, 0, 255, 0, 100);
}

bool lightSensorOk(int lightVal) {
  if (lightVal <= 2 || lightVal >= 1021) {
    if (lightBadSince == 0) lightBadSince = millis();
    if (millis() - lightBadSince > 1500) return false;
  } else {
    lightBadSince = 0;
  }
  return true;
}

bool updateLightLed(int &lightValOut, int &lightPercentOut, int &ledPercentOut, int &ledPwmOut) {
  int lightVal = analogRead(lightPin);

  int lightPercent = map(lightVal, LIGHT_RAW_MIN, LIGHT_RAW_MAX, 0, 100);
  lightPercent = constrain(lightPercent, 0, 100);

  int ledPercent = 100 - lightPercent;
  int ledBrightness = map(ledPercent, 0, 100, 0, 255);
  ledBrightness = constrain(ledBrightness, 0, 255);

  analogWrite(ledPin, ledBrightness);

  lightValOut = lightVal;
  lightPercentOut = lightPercent;
  ledPercentOut = ledPercent;
  ledPwmOut = ledBrightness;

  return lightSensorOk(lightVal);
}


bool ledCableOk_D3_to_D4(int restorePwm) {
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, HIGH);
  delay(3);
  int a = digitalRead(ledSensePin);

  digitalWrite(ledPin, LOW);
  delay(3);
  int b = digitalRead(ledSensePin);

  analogWrite(ledPin, restorePwm);

  return (a != b);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  dht20.begin();
  bmp280.init();
  rtc.begin();

  pinMode(buttonPin, INPUT);
  pinMode(potPin, INPUT);
  pinMode(lightPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  pinMode(ledSensePin, INPUT);

  noTone(buzzerPin);

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setContrast(currentContrast);

  drawMenu();
}

void loop() {
  updateBeep();

  int lightVal = 0, lightPercent = 0, ledPercent = 0, ledPwm = 0;
  bool lightOk = updateLightLed(lightVal, lightPercent, ledPercent, ledPwm);

  bool errorNow = false;

  bool rtcOk = rtcPresent();
  if (!rtcOk) errorNow = true;
  else if (!rtc.isrunning()) errorNow = true;

  if (!lightOk) errorNow = true;

  if (millis() - lastLedCheck > ledCheckInterval) {
    lastLedCheck = millis();
    ledCableOkCached = ledCableOk_D3_to_D4(ledPwm);
  }
  if (!ledCableOkCached) errorNow = true;

  if (errorNow) startBeep();

  bool button = digitalRead(buttonPin);

  if (button == HIGH && lastButton == LOW && millis() - lastButtonTime > buttonDelay) {
    lastButtonTime = millis();
    inMeasure = !inMeasure;
    if (inMeasure) {
      if (selected == 0) drawClimateLayout();
      else if (selected == 1) drawPressureLayout();
      else if (selected == 2) drawRtcLayout();
      else if (selected == 3) drawBrightnessLayout();
      else if (selected == 4) drawLightLayout();
    } else {
      drawMenu();
    }
  }

  lastButton = button;

  if (!inMeasure) {
    int pot = analogRead(potPin);
    int newSelected = map(pot, 0, 1023, 0, menuCount - 1);
    newSelected = constrain(newSelected, 0, menuCount - 1);
    if (newSelected != selected) {
      selected = newSelected;
      drawMenu();
    }
    delay(50);
    return;
  }

  if (selected == 4) {
    u8x8.setCursor(9, 2);
    u8x8.print("      ");
    u8x8.setCursor(9, 2);
    u8x8.print(lightVal);

    u8x8.setCursor(8, 4);
    u8x8.print("      ");
    u8x8.setCursor(8, 4);
    if (lightPercent < 100) u8x8.print(' ');
    if (lightPercent < 10) u8x8.print(' ');
    u8x8.print(lightPercent);
    u8x8.print("%");

    clearLineFrom(5, 0);
    clearLineFrom(6, 0);

    const uint8_t barLen = 12;
    uint8_t filled = (uint16_t)lightPercent * barLen / 100;

    u8x8.setCursor(0, 5);
    u8x8.print("[");
    for (uint8_t i = 0; i < barLen; i++) u8x8.print(i < filled ? '#' : '-');
    u8x8.print("]");

    u8x8.setCursor(0, 7);
    if (!ledCableOkCached) u8x8.print("Status: LED?   ");
    else if (lightOk) u8x8.print("Status: OK     ");
    else u8x8.print("Status: BLAD   ");

    delay(200);
    return;
  }

  if (selected == 3) {
    int potRaw = analogRead(potPin);
    if (!brightnessMoved) {
      if (abs(potRaw - lastPotRaw) >= 8) brightnessMoved = true;
      drawBrightnessBar(currentPercentFromContrast());
      u8x8.setCursor(0, 6);
      u8x8.print("Status: OK     ");
      delay(50);
      return;
    }

    uint8_t percent = map(potRaw, 0, 1023, 0, 100);
    uint8_t x = map(potRaw, 0, 1023, 0, 255);
    uint16_t y = (uint16_t)x * x;
    uint8_t contrast = (uint8_t)(y / 255);

    if (contrast != currentContrast) {
      currentContrast = contrast;
      u8x8.setContrast(currentContrast);
    }

    drawBrightnessBar(percent);
    u8x8.setCursor(0, 6);
    u8x8.print("Status: OK     ");
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
      startBeep();
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
      startBeep();
    }
  }
  else if (selected == 2) {
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
      startBeep();
    }
    else {
      if (!rtc.isrunning()) {
        u8x8.setCursor(0, 6);
        u8x8.print("Status: USTAW  ");
        startBeep();
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

  delay(1000);
}
