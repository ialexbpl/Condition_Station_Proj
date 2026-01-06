# 🌡️ Condition Station

Stacja pogodowa i środowiskowa oparta na **Arduino Grove Beginner Kit** z aplikacją desktopową w języku C.



## 📋 Spis treści

- [Opis projektu](#-opis-projektu)
- [Funkcje](#-funkcje)
- [Wymagania sprzętowe](#-wymagania-sprzętowe)
- [Wymagania programowe](#-wymagania-programowe)
- [Instalacja](#-instalacja)
- [Użytkowanie](#-użytkowanie)
- [Komunikacja Serial](#-komunikacja-serial)
- [Autorzy](#-autorzy)
- [Licencja](#-licencja)

---

## 📖 Opis projektu

**Condition Station** to kompleksowy system monitorowania środowiska składający się z:

1. **Firmware Arduino** - odczytuje dane z czujników i wyświetla na OLED
2. **Aplikacja desktopowa (C/Win32)** - wizualizacja danych na komputerze z funkcją budzika i logowania CSV

System mierzy temperaturę, wilgotność, ciśnienie atmosferyczne, natężenie światła oraz poziom dźwięku.

---

## ✨ Funkcje

### Arduino (OLED Menu)

| Funkcja | Opis |
|---------|------|
| 🌡️ **Pomiar klimatu** | Temperatura i wilgotność (DHT20) |
| 🔵 **Ciśnienie** | Ciśnienie atmosferyczne (BMP280) |
| 🕐 **Zegar RTC** | Data i godzina z walidacją |
| 💡 **Jasność OLED** | Regulacja kontrastu potencjometrem |
| ☀️ **Światło** | Pomiar natężenia światła + sterowanie LED PWM |
| ⏰ **Budzik** | 3 melodie, wyłączanie głosem/dźwiękiem |

### Aplikacja desktopowa (Windows)

| Funkcja | Opis |
|---------|------|
| 📊 **Wizualizacja** | Wyświetlanie wszystkich danych w czasie rzeczywistym |
| 🎨 **Kolorowanie** | Temperatura: niebieski (zimno), zielony (OK), czerwony (gorąco) |
| ⏰ **Zdalne sterowanie budzikiem** | Ustawianie godziny, melodii, progu dźwięku |
| 📝 **Logowanie CSV** | Zapis danych co minutę do pliku CSV |
| 🔌 **Auto-wykrywanie portów** | Skanowanie dostępnych portów COM |

---

## 🔧 Wymagania sprzętowe

### Grove Beginner Kit for Arduino

Projekt wykorzystuje wbudowane czujniki płytki:

| Pin | Komponent | Funkcja |
|-----|-----------|---------|
| A0 | Potencjometr | Nawigacja menu / regulacja jasności |
| A2 | Mikrofon | Detekcja dźwięku (wyłączanie budzika) |
| A6 | Czujnik światła | Pomiar natężenia światła |
| D3 | LED (PWM) | Automatyczne oświetlenie |
| D4 | LED Sense | Detekcja podłączenia LED |
| D5 | Buzzer | Sygnały dźwiękowe i budzik |
| D6 | Przycisk | Nawigacja menu |
| I2C | OLED 128x64 | Wyświetlacz |
| I2C | DHT20 | Temperatura i wilgotność |
| I2C | BMP280 | Ciśnienie atmosferyczne |
| I2C | DS1307 RTC | Zegar czasu rzeczywistego |

---

## 💻 Wymagania programowe

### Arduino IDE
- Arduino IDE 1.8+ lub Arduino IDE 2.0+
- Biblioteki:
  - `U8g2` (dla U8x8)
  - `DHT20` (Grove)
  - `Seeed_BMP280`
  - `RTClib` (Adafruit)

### Aplikacja desktopowa
- Windows 10/11
- Kompilator GCC (MinGW-w64) lub Visual Studio

---

## 📥 Instalacja

### 1. Firmware Arduino

```bash
# Otwórz Arduino IDE
# Zainstaluj wymagane biblioteki przez Library Manager
# Otwórz Condition_Station_Proj.ino
# Wybierz płytkę: Arduino Uno (lub Seeeduino Lotus)
# Wgraj na płytkę
```

### 2. Aplikacja desktopowa

#### Opcja A: GCC (MinGW)
```bash
cd ConditionStation_C
gcc condition_station.c -o condition_station.exe -lgdi32 -mwindows
```

#### Opcja B: Visual Studio
```bash
cd ConditionStation_C
cl /O2 /Fe:condition_station.exe condition_station.c user32.lib gdi32.lib
```

#### Opcja C: Użyj skryptu
```bash
cd ConditionStation_C
kompiluj.bat
```

---

## 🎮 Użytkowanie

### Menu Arduino (OLED)

1. **Potencjometrem** wybierz opcję menu
2. **Przyciskiem** wejdź/wyjdź z pomiaru
3. W trybie pomiaru dane odświeżają się automatycznie

### Aplikacja desktopowa

1. Uruchom `condition_station.exe`
2. Wybierz port COM z listy
3. Kliknij **"Połącz"**
4. Dane pojawią się automatycznie

#### Budzik
1. Ustaw godzinę (HH:MM)
2. Wybierz melodię (1-3)
3. Ustaw próg dźwięku (do wyłączania głosem)
4. Zaznacz "Aktywny" i kliknij "Ustaw"

#### Logowanie CSV
1. Kliknij **"Start CSV"** - rozpoczyna nagrywanie
2. Dane zapisywane są co 1 minutę
3. Kliknij **"Stop CSV"** - zatrzymuje i zapisuje plik


## 📡 Komunikacja Serial

### Format danych (Arduino → PC)
JSON wysyłany co 500ms:
```json
{
  "temp": 23.5,
  "hum": 45.2,
  "pressure": 1013,
  "light": 67,
  "lightRaw": 520,
  "soundRaw": 123,
  "alarmActive": 0,
  "time": "14:30:25",
  "date": "06.01.2026"
}
```

### Komendy (PC → Arduino)

| Komenda | Format | Opis |
|---------|--------|------|
| Ustaw budzik | `ALARM;HH;MM;enabled;melody;threshold\n` | Konfiguracja budzika |
| Stop alarm | `STOP\n` | Zatrzymuje dzwoniący alarm |

Przykład: `ALARM;07;30;1;2;650\n` - budzik na 7:30, włączony, melodia 2, próg 650

---

## 📊 Format pliku CSV

```csv
Data,Czas,Temperatura,Wilgotnosc,Cisnienie,Swiatlo,LightRaw,SoundRaw,AlarmActive
2026-01-06,14:30:00,23.5,45.2,1013,67,520,123,0
2026-01-06,14:31:00,23.6,45.1,1013,65,510,118,0
```

---

## 👥 Autorzy

- **Jakub Aposte**
- **Alexander Buczek**
- **Miłosz Dobrowolski**

Projekt stworzony w ramach zajęć z systemów wbudowanych.

---

## 📄 Licencja

Ten projekt jest udostępniony na licencji **MIT**.

```
MIT License

Copyright (c) 2025 Jakub Aposte, Alexander Buczek, Miłosz Dobrowolski

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

## 🔗 Linki

- [Grove Beginner Kit](https://wiki.seeedstudio.com/Grove-Beginner-Kit-For-Arduino/)
- [Arduino IDE](https://www.arduino.cc/en/software)
- [MinGW-w64](https://www.mingw-w64.org/)

---

> 💡 **Tip:** Użyj Serial Monitor w Arduino IDE (9600 baud) aby zobaczyć surowe dane JSON.

