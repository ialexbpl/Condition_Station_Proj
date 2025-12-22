#include <Arduino.h>
#include <Wire.h>
#include <U8x8lib.h>
#include <DHT20.h>

DHT20 dht20;
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

const int buttonPin = 6;

// stany
bool showMeasure = false;
bool lastButton = LOW;

// proste opóźnienie przycisku
unsigned long lastButtonTime = 0;
unsigned long buttonDelay = 200;

void printPaddedFloat(uint8_t col, uint8_t row, float val, uint8_t decimals, uint8_t width) {
  u8x8.setCursor(col, row);
  for (uint8_t i = 0; i < width; i++) u8x8.print(' ');
  u8x8.setCursor(col, row);
  u8x8.print(val, decimals);
}

// -------- MENU --------
void drawMenu() {
  u8x8.clearDisplay();
  u8x8.setCursor(2, 3);
  u8x8.print("POMIAR KLIMATU");
}

void drawMeasureLayout() {
  u8x8.clearDisplay();
  u8x8.setCursor(0, 0);
  u8x8.print("POMIAR KLIMATU");

  u8x8.setCursor(0, 2);
  u8x8.print("Temp: ");
  u8x8.setCursor(0, 4);
  u8x8.print("Humi: ");

  u8x8.setCursor(14, 2);
  u8x8.print("C");
  u8x8.setCursor(14, 4);
  u8x8.print("%");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  dht20.begin();

  pinMode(buttonPin, INPUT);

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  drawMenu();
}


void loop() {

  bool button = digitalRead(buttonPin);

  if (button == HIGH && lastButton == LOW &&
      millis() - lastButtonTime > buttonDelay) {

    lastButtonTime = millis();
    showMeasure = !showMeasure;

    if (showMeasure) {
      drawMeasureLayout();
    } else {
      drawMenu();
    }
  }

  lastButton = button;

  if (showMeasure) {
    int status = dht20.read();

    if (status == 0) {
      float temp = dht20.getTemperature();
      float humi = dht20.getHumidity();

      printPaddedFloat(6, 2, temp, 1, 7);
      printPaddedFloat(6, 4, humi, 1, 7);

      u8x8.setCursor(0, 6);
      u8x8.print("Status: OK     ");
    } else {
      u8x8.setCursor(6, 2);
      u8x8.print("BLAD   ");
      u8x8.setCursor(6, 4);
      u8x8.print("BLAD   ");
      u8x8.setCursor(0, 6);
      u8x8.print("Status: BLAD   ");
    }
  }

  delay(1000);
}