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
const int soundPin = A2;
const int ledPin = 3;
const int ledSensePin = 4;

int selected = 0;
bool inMeasure = false;

int potFiltered = 0;
bool potInit = false;

bool lastButtonStable = LOW;
bool lastButtonRead = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceMs = 25;

unsigned long lastBeepTime = 0;
const unsigned long beepCooldown = 3000;
bool beeping = false;
unsigned long beepStart = 0;
const unsigned long beepDuration = 2000;

const uint16_t melody1Notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
const uint16_t melody2Notes[] = {523, 494, 440, 392, 349, 330, 294, 262};
const uint16_t melody3Notes[] = {392, 0, 392, 0, 440, 0, 523, 0};
const uint8_t melody1Len = sizeof(melody1Notes) / sizeof(melody1Notes[0]);
const uint8_t melody2Len = sizeof(melody2Notes) / sizeof(melody2Notes[0]);
const uint8_t melody3Len = sizeof(melody3Notes) / sizeof(melody3Notes[0]);
const unsigned long alarmNoteInterval = 200;
const unsigned long soundStopHoldMs = 250;

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;
bool alarmEnabled = false;
uint8_t alarmMelody = 1;
uint16_t alarmSoundThreshold = 650;
bool alarmRinging = false;
uint8_t alarmNoteIndex = 0;
unsigned long alarmNoteStart = 0;
uint32_t alarmLastDateKey = 0;
unsigned long soundAboveSince = 0;

unsigned long lastSoundRead = 0;
const unsigned long soundReadInterval = 50;
int soundRaw = 0;

void startBeep() {
  if (alarmRinging) return;
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
    if (!alarmRinging) noTone(buzzerPin);
  }
}

uint8_t getMelodyLength(uint8_t melody) {
  if (melody == 2) return melody2Len;
  if (melody == 3) return melody3Len;
  return melody1Len;
}

uint16_t getMelodyNote(uint8_t melody, uint8_t idx) {
  if (melody == 2) return melody2Notes[idx % melody2Len];
  if (melody == 3) return melody3Notes[idx % melody3Len];
  return melody1Notes[idx % melody1Len];
}

void stopAlarm() {
  alarmRinging = false;
  alarmNoteIndex = 0;
  alarmNoteStart = 0;
  soundAboveSince = 0;
  if (!beeping) noTone(buzzerPin);
}

void startAlarm() {
  if (alarmRinging) return;
  alarmRinging = true;
  alarmNoteIndex = 0;
  alarmNoteStart = 0;
  soundAboveSince = 0;
}

void handleAlarmCommand(char* cmd) {
  char* token = strtok(cmd, ";");
  if (!token || strcmp(token, "ALARM") != 0) return;

  token = strtok(NULL, ";");
  if (!token) return;
  int hour = atoi(token);

  token = strtok(NULL, ";");
  if (!token) return;
  int minute = atoi(token);

  token = strtok(NULL, ";");
  if (!token) return;
  int enabled = atoi(token);

  token = strtok(NULL, ";");
  if (!token) return;
  int melody = atoi(token);

  token = strtok(NULL, ";");
  if (!token) return;
  int threshold = atoi(token);

  hour = constrain(hour, 0, 23);
  minute = constrain(minute, 0, 59);
  melody = constrain(melody, 1, 3);
  threshold = constrain(threshold, 0, 1023);

  alarmHour = (uint8_t)hour;
  alarmMinute = (uint8_t)minute;
  alarmEnabled = (enabled != 0);
  alarmMelody = (uint8_t)melody;
  alarmSoundThreshold = (uint16_t)threshold;
  if (!alarmEnabled) stopAlarm();
}

void processSerialCommands() {
  static char cmdBuffer[64];
  static uint8_t cmdPos = 0;

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdPos > 0) {
        cmdBuffer[cmdPos] = '\0';
        if (strncmp(cmdBuffer, "ALARM;", 6) == 0) {
          handleAlarmCommand(cmdBuffer);
        } else if (strcmp(cmdBuffer, "STOP") == 0) {
          stopAlarm();
        }
        cmdPos = 0;
      }
    } else if (cmdPos < sizeof(cmdBuffer) - 1) {
      cmdBuffer[cmdPos++] = c;
    }
  }
}

uint8_t currentContrast = 120;
bool brightnessMoved = false;
int lastPotRaw = 0;

int LIGHT_RAW_MIN = 0;
int LIGHT_RAW_MAX = 775;

unsigned long lightBadSince = 0;

unsigned long lastLightRead = 0;
const unsigned long lightReadInterval = 100;

int lightRaw = 0;
int lightPercent = 0;
int ledPwm = 0;
bool lightOk = true;

unsigned long lastLightScreenUpdate = 0;
const unsigned long lightScreenInterval = 200;

unsigned long lastDhtRead = 0;
const unsigned long dhtInterval = 2000;
float tempC = NAN;
float humP = NAN;
int dhtStatus = -1;

unsigned long lastBmpRead = 0;
const unsigned long bmpInterval = 1000;
int pressureInt = 0;
bool pressureOk = false;

unsigned long lastRtcRead = 0;
const unsigned long rtcInterval = 200;
char timeBuf[16] = "--:--:--";
char dateBuf[16] = "--.--.----";
bool rtcOk = false;
bool rtcNowValid = false;
uint8_t rtcHour = 0;
uint8_t rtcMinute = 0;
uint8_t rtcSecond = 0;
uint8_t rtcDay = 1;
uint8_t rtcMonth = 1;
uint16_t rtcYear = 2000;

unsigned long lastSerialSend = 0;
const unsigned long serialInterval = 500;

bool rtcPresent() {
  Wire.beginTransmission(0x68);
  return (Wire.endTransmission() == 0);
}

void printPaddedFloat(uint8_t col, uint8_t row, float val, uint8_t decimals, uint8_t width) {
  u8x8.setCursor(col, row);
  for (uint8_t i = 0; i < width; i++) u8x8.print(' ');
  u8x8.setCursor(col, row);
  if (isnan(val)) u8x8.print("--.-");
  else u8x8.print(val, decimals);
}

void clearLineFrom(uint8_t row, uint8_t startCol) {
  u8x8.setCursor(startCol, row);
  for (uint8_t i = startCol; i < 16; i++) u8x8.print(' ');
}

unsigned long lastLedCheck = 0;
const unsigned long ledCheckInterval = 1500;
bool ledCableOk = true;

bool ledCableOk_D3_to_D4(int restorePwm) {
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, HIGH);
  delay(2);
  int a = digitalRead(ledSensePin);

  digitalWrite(ledPin, LOW);
  delay(2);
  int b = digitalRead(ledSensePin);

  analogWrite(ledPin, restorePwm);
  return (a != b);
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
    u8x8.print(idx == selected ? "> " : "  ");
    u8x8.print(menuItems[idx]);
  }
}

void drawClimateLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(1, 0); u8x8.print("POMIAR KLIMATU");
  u8x8.setCursor(0, 2); u8x8.print("Temp:");
  u8x8.setCursor(0, 4); u8x8.print("Wilg:");
  u8x8.setCursor(14, 2); u8x8.print("C");
  u8x8.setCursor(14, 4); u8x8.print("%");
  u8x8.setCursor(0, 6); u8x8.print("Status: ----   ");
}

void drawPressureLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(3, 0); u8x8.print("CISNIENIE");
  u8x8.setCursor(0, 2); u8x8.print("hPa:");
  u8x8.setCursor(0, 4); u8x8.print("Status: ----   ");
}

void drawRtcLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(5, 0); u8x8.print("ZEGAR");
  u8x8.setCursor(0, 2); u8x8.print("Godz:");
  u8x8.setCursor(0, 4); u8x8.print("Data:");
  u8x8.setCursor(0, 6); u8x8.print("Status: ----   ");
}

void drawBrightnessLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(2, 0); u8x8.print("JASNOSC OLED");
  u8x8.setCursor(0, 2); u8x8.print("Poziom:");
  u8x8.setCursor(0, 6); u8x8.print("Status: OK     ");
  brightnessMoved = false;
  lastPotRaw = analogRead(potPin);
}

void drawLightLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(3, 0); u8x8.print("SWIATLO");
  u8x8.setCursor(0, 2); u8x8.print("Wartosc:");
  u8x8.setCursor(0, 4); u8x8.print("Poziom:");
  u8x8.setCursor(0, 7); u8x8.print("Status: ----   ");
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

int potToMenuIndex(int val) {
  int idx = (val * menuCount) / 1024;
  if (idx < 0) idx = 0;
  if (idx > menuCount - 1) idx = menuCount - 1;
  return idx;
}

void updateMenuSelection() {
  int raw = analogRead(potPin);

  if (!potInit) {
    potFiltered = raw;
    potInit = true;
  } else {
    potFiltered = (potFiltered * 7 + raw) / 8;
  }

  int newSelected = potToMenuIndex(potFiltered);
  if (newSelected != selected) {
    selected = newSelected;
    drawMenu();
  }
}

void updateButton() {
  bool reading = digitalRead(buttonPin);

  if (reading != lastButtonRead) {
    lastDebounceTime = millis();
    lastButtonRead = reading;
  }

  if (millis() - lastDebounceTime >= debounceMs) {
    if (reading != lastButtonStable) {
      lastButtonStable = reading;

      if (lastButtonStable == HIGH) {
        inMeasure = !inMeasure;

        if (inMeasure) {
          if (selected == 0) drawClimateLayout();
          else if (selected == 1) drawPressureLayout();
          else if (selected == 2) drawRtcLayout();
          else if (selected == 3) drawBrightnessLayout();
          else if (selected == 4) drawLightLayout();
        } else {
          analogWrite(ledPin, 0);
          drawMenu();
        }
      }
    }
  }
}


bool lightSensorOk(int v) {

  if (v >= 1021) {
    if (lightBadSince == 0) lightBadSince = millis();
    if (millis() - lightBadSince > 1500) return false;
  } else {
    lightBadSince = 0;
  }
  return true;
}


void updateLight() {
  if (millis() - lastLightRead < lightReadInterval) return;
  lastLightRead = millis();

  lightRaw = analogRead(lightPin);

  lightPercent = map(lightRaw, LIGHT_RAW_MIN, LIGHT_RAW_MAX, 0, 100);
  lightPercent = constrain(lightPercent, 0, 100);

  int ledPercent = 100 - lightPercent;
  ledPwm = map(ledPercent, 0, 100, 0, 255);
  ledPwm = constrain(ledPwm, 0, 255);


  analogWrite(ledPin, ledPwm);

  lightOk = lightSensorOk(lightRaw);

  if (millis() - lastLedCheck >= ledCheckInterval) {
    lastLedCheck = millis();
    ledCableOk = ledCableOk_D3_to_D4(ledPwm);
  }
}

void updateSound() {
  if (millis() - lastSoundRead < soundReadInterval) return;
  lastSoundRead = millis();
  soundRaw = analogRead(soundPin);
}

void updateDht() {
  if (millis() - lastDhtRead < dhtInterval) return;
  lastDhtRead = millis();

  int st = dht20.read();
  dhtStatus = st;
  if (st == 0) {
    tempC = dht20.getTemperature();
    humP = dht20.getHumidity();
  }
}

void updateBmp() {
  if (millis() - lastBmpRead < bmpInterval) return;
  lastBmpRead = millis();

  float p = bmp280.getPressure() / 100.0f;
  if (p > 0) {
    pressureInt = (int)(p + 0.5f);
    pressureOk = true;
  } else {
    pressureOk = false;
    pressureInt = 0;
  }
}

void updateRtc() {
  if (millis() - lastRtcRead < rtcInterval) return;
  lastRtcRead = millis();

  rtcOk = (rtcPresent() && rtc.isrunning());
  if (rtcOk) {
    DateTime now = rtc.now();
    rtcNowValid = true;
    rtcHour = now.hour();
    rtcMinute = now.minute();
    rtcSecond = now.second();
    rtcDay = now.day();
    rtcMonth = now.month();
    rtcYear = now.year();
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d", now.day(), now.month(), now.year());
  } else {
    rtcNowValid = false;
    snprintf(timeBuf, sizeof(timeBuf), "%s", "--:--:--");
    snprintf(dateBuf, sizeof(dateBuf), "%s", "--.--.----");
  }
}

void updateAlarm() {
  if (!alarmEnabled && alarmRinging) {
    stopAlarm();
    return;
  }

  if (!alarmEnabled) return;

  if (!alarmRinging) {
    if (!rtcNowValid) return;
    uint32_t dateKey = (uint32_t)rtcYear * 10000UL + (uint32_t)rtcMonth * 100UL + rtcDay;
    if (rtcHour == alarmHour && rtcMinute == alarmMinute && dateKey != alarmLastDateKey) {
      alarmLastDateKey = dateKey;
      startAlarm();
    } else {
      return;
    }
  }

  if (alarmNoteStart == 0 || millis() - alarmNoteStart >= alarmNoteInterval) {
    alarmNoteStart = millis();
    uint8_t len = getMelodyLength(alarmMelody);
    uint16_t note = getMelodyNote(alarmMelody, alarmNoteIndex);
    alarmNoteIndex = (alarmNoteIndex + 1) % len;
    if (note > 0) tone(buzzerPin, note);
    else noTone(buzzerPin);
  }

  if (alarmSoundThreshold > 0) {
    if (soundRaw >= alarmSoundThreshold) {
      if (soundAboveSince == 0) soundAboveSince = millis();
      else if (millis() - soundAboveSince >= soundStopHoldMs) stopAlarm();
    } else {
      soundAboveSince = 0;
    }
  }
}

void sendSerialData() {
  if (millis() - lastSerialSend < serialInterval) return;
  lastSerialSend = millis();

  float tempOut = tempC;
  float humOut = humP;
  if (tempOut != tempOut) tempOut = 0.0f;
  if (humOut != humOut) humOut = 0.0f;

  Serial.print("{\"temp\":");
  Serial.print(tempOut, 1);
  Serial.print(",\"hum\":");
  Serial.print(humOut, 1);
  Serial.print(",\"pressure\":");
  Serial.print(pressureInt);
  Serial.print(",\"light\":");
  Serial.print(lightPercent);
  Serial.print(",\"lightRaw\":");
  Serial.print(lightRaw);
  Serial.print(",\"soundRaw\":");
  Serial.print(soundRaw);
  Serial.print(",\"alarmActive\":");
  Serial.print(alarmRinging ? 1 : 0);
  Serial.print(",\"time\":\"");
  Serial.print(timeBuf);
  Serial.print("\",\"date\":\"");
  Serial.print(dateBuf);
  Serial.println("\"}");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(100000);

  dht20.begin();
  bmp280.init();
  rtc.begin();

  pinMode(buttonPin, INPUT);
  pinMode(potPin, INPUT);
  pinMode(lightPin, INPUT);
  pinMode(soundPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(ledSensePin, INPUT);

  noTone(buzzerPin);
  analogWrite(ledPin, 0);

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setContrast(currentContrast);

  drawMenu();
}

void loop() {
  updateBeep();
  updateButton();
  processSerialCommands();

  updateSound();
  updateLight();
  updateDht();
  updateBmp();
  updateRtc();
  updateAlarm();

  bool errorNow = false;
  if (!rtcOk) errorNow = true;
  if (!ledCableOk) errorNow = true;
  if (!lightOk) errorNow = true;

  if (errorNow) startBeep();

  sendSerialData();

  if (!inMeasure) {
    updateMenuSelection();
    return;
  }

  if (selected == 0) {
    printPaddedFloat(6, 2, tempC, 1, 7);
    printPaddedFloat(6, 4, humP, 1, 7);
    u8x8.setCursor(0, 6);
    if (dhtStatus == 0) u8x8.print("Status: OK     ");
    else u8x8.print("Status: BLAD   ");
    return;
  }

  if (selected == 1) {
    if (pressureOk) {
      u8x8.setCursor(4, 2);
      u8x8.print("        ");
      u8x8.setCursor(4, 2);
      u8x8.print(pressureInt);
      u8x8.setCursor(0, 4);
      u8x8.print("Status: OK     ");
    } else {
      u8x8.setCursor(4, 2);
      u8x8.print("BLAD    ");
      u8x8.setCursor(0, 4);
      u8x8.print("Status: BLAD   ");
    }
    return;
  }

  if (selected == 2) {
    const uint8_t valueCol = 6;
    u8x8.setCursor(0, 6);
    if (rtcOk) u8x8.print("Status: OK     ");
    else u8x8.print("Status: BLAD   ");

    clearLineFrom(2, valueCol);
    u8x8.setCursor(valueCol, 2);
    u8x8.print(timeBuf);

    clearLineFrom(4, valueCol);
    u8x8.setCursor(valueCol, 4);
    u8x8.print(dateBuf);
    return;
  }

  if (selected == 3) {
    int potRaw = analogRead(potPin);

    if (!brightnessMoved) {
      if (abs(potRaw - lastPotRaw) >= 8) brightnessMoved = true;
      drawBrightnessBar(map(currentContrast, 0, 255, 0, 100));
      u8x8.setCursor(0, 6);
      u8x8.print("Status: OK     ");
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
    return;
  }

  if (selected == 4) {
    if (millis() - lastLightScreenUpdate < lightScreenInterval) return;
    lastLightScreenUpdate = millis();

    u8x8.setCursor(9, 2);
    u8x8.print("      ");
    u8x8.setCursor(9, 2);
    u8x8.print(lightRaw);

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
    if (!ledCableOk) u8x8.print("Status: LED?   ");
    else if (lightOk) u8x8.print("Status: OK     ");
    else u8x8.print("Status: BLAD   ");

    return;
  }
}
