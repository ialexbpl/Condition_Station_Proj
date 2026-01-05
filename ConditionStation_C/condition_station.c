/*
 * Condition Station - Desktop App (C/Win32)
 * Wyswietla dane z Arduino Grove Beginner Kit
 * Kompilacja: gcc condition_station.c -o condition_station.exe -lgdi32 -mwindows
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TIMER_ID 1
#define TIMER_INTERVAL 100
#define ID_BTN_SET_ALARM 201
#define ID_BTN_STOP_ALARM 202

// Dane z czujnikow
typedef struct {
    float temp;
    float hum;
    int pressure;
    int light;
    int lightRaw;
    int soundRaw;
    int alarmActive;
    char time[16];
    char date[16];
} SensorData;

// Globalne zmienne
HANDLE hSerial = INVALID_HANDLE_VALUE;
SensorData sensorData = {0, 0, 0, 0, 0, 0, 0, "--:--:--", "--.--.----"};
char comPort[16] = "COM3";
BOOL isConnected = FALSE;
char statusText[64] = "Rozlaczono";

// Kolory
COLORREF COLOR_BG = RGB(30, 30, 40);
COLORREF COLOR_TEXT = RGB(255, 255, 255);
COLORREF COLOR_ACCENT = RGB(0, 212, 255);
COLORREF COLOR_GREEN = RGB(0, 255, 136);
COLORREF COLOR_RED = RGB(255, 68, 68);
COLORREF COLOR_BLUE = RGB(0, 191, 255);
COLORREF COLOR_GRAY = RGB(128, 128, 128);

// Kontrolki
HWND hComboPort, hBtnConnect, hStatusLabel;
HWND hEditHour, hEditMinute, hCheckAlarm, hComboMelody, hEditThreshold;
HWND hBtnSetAlarm, hBtnStopAlarm, hLabelAlarmStatus, hLabelSound;

// Funkcje
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OpenSerialPort(HWND hwnd);
void CloseSerialPort(void);
void ReadSerialData(void);
void ParseJSON(const char* json);
void DrawSensorData(HDC hdc, RECT* rect);
void EnumeratePorts(HWND hCombo);
void SendSerialCommand(const char* cmd);
void UpdateAlarmUi(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "ConditionStationClass";
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassA(&wc);
    
    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "Condition Station",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 720,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    CloseSerialPort();
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Tytul
            CreateWindowA("STATIC", "CONDITION STATION",
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                0, 15, 435, 30, hwnd, NULL, NULL, NULL);
            
            // Port COM combo
            hComboPort = CreateWindowA("COMBOBOX", "",
                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                50, 55, 150, 200, hwnd, (HMENU)101, NULL, NULL);
            EnumeratePorts(hComboPort);
            
            // Przycisk polacz
            hBtnConnect = CreateWindowA("BUTTON", "Polacz",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                210, 55, 100, 25, hwnd, (HMENU)102, NULL, NULL);
            
            // Przycisk odswiez
            CreateWindowA("BUTTON", "Odswiez",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                320, 55, 70, 25, hwnd, (HMENU)103, NULL, NULL);
            
            // Status
            hStatusLabel = CreateWindowA("STATIC", "Status: Rozlaczono",
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                0, 90, 435, 20, hwnd, NULL, NULL, NULL);

            // Budzik
            int alarmY = 540;
            CreateWindowA("STATIC", "BUDZIK",
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                0, alarmY, 435, 20, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Godzina:",
                WS_VISIBLE | WS_CHILD,
                30, alarmY + 25, 60, 20, hwnd, NULL, NULL, NULL);
            hEditHour = CreateWindowA("EDIT", "07",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
                95, alarmY + 22, 28, 22, hwnd, NULL, NULL, NULL);
            CreateWindowA("STATIC", ":",
                WS_VISIBLE | WS_CHILD,
                127, alarmY + 25, 8, 20, hwnd, NULL, NULL, NULL);
            hEditMinute = CreateWindowA("EDIT", "00",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
                135, alarmY + 22, 28, 22, hwnd, NULL, NULL, NULL);

            hCheckAlarm = CreateWindowA("BUTTON", "Aktywny",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                175, alarmY + 22, 80, 22, hwnd, NULL, NULL, NULL);

            hBtnStopAlarm = CreateWindowA("BUTTON", "Stop",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                265, alarmY + 22, 60, 22, hwnd, (HMENU)ID_BTN_STOP_ALARM, NULL, NULL);

            CreateWindowA("STATIC", "Melodia:",
                WS_VISIBLE | WS_CHILD,
                30, alarmY + 55, 60, 20, hwnd, NULL, NULL, NULL);
            hComboMelody = CreateWindowA("COMBOBOX", "",
                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                95, alarmY + 52, 60, 200, hwnd, NULL, NULL, NULL);
            SendMessageA(hComboMelody, CB_ADDSTRING, 0, (LPARAM)"1");
            SendMessageA(hComboMelody, CB_ADDSTRING, 0, (LPARAM)"2");
            SendMessageA(hComboMelody, CB_ADDSTRING, 0, (LPARAM)"3");
            SendMessageA(hComboMelody, CB_SETCURSEL, 0, 0);

            CreateWindowA("STATIC", "Prog dzwieku:",
                WS_VISIBLE | WS_CHILD,
                170, alarmY + 55, 90, 20, hwnd, NULL, NULL, NULL);
            hEditThreshold = CreateWindowA("EDIT", "650",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
                260, alarmY + 52, 60, 22, hwnd, NULL, NULL, NULL);

            hBtnSetAlarm = CreateWindowA("BUTTON", "Ustaw",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                330, alarmY + 52, 70, 22, hwnd, (HMENU)ID_BTN_SET_ALARM, NULL, NULL);

            hLabelSound = CreateWindowA("STATIC", "Dzwiek: ---",
                WS_VISIBLE | WS_CHILD,
                30, alarmY + 82, 140, 20, hwnd, NULL, NULL, NULL);
            hLabelAlarmStatus = CreateWindowA("STATIC", "Alarm: OFF",
                WS_VISIBLE | WS_CHILD,
                200, alarmY + 82, 200, 20, hwnd, NULL, NULL, NULL);
            
            // Timer do odczytu danych
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);
            return 0;
        }
        
        case WM_TIMER:
            if (wParam == TIMER_ID && isConnected) {
                ReadSerialData();
                UpdateAlarmUi();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == 102) {  // Przycisk Polacz
                if (isConnected) {
                    CloseSerialPort();
                    SetWindowTextA(hBtnConnect, "Polacz");
                    SetWindowTextA(hStatusLabel, "Status: Rozlaczono");
                } else {
                    int idx = SendMessageA(hComboPort, CB_GETCURSEL, 0, 0);
                    if (idx != CB_ERR) {
                        SendMessageA(hComboPort, CB_GETLBTEXT, idx, (LPARAM)comPort);
                        OpenSerialPort(hwnd);
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else if (LOWORD(wParam) == 103) {  // Przycisk Odswiez
                EnumeratePorts(hComboPort);
            }
            else if (LOWORD(wParam) == ID_BTN_SET_ALARM) {
                char hourBuf[8] = {0};
                char minuteBuf[8] = {0};
                char threshBuf[8] = {0};

                GetWindowTextA(hEditHour, hourBuf, sizeof(hourBuf));
                GetWindowTextA(hEditMinute, minuteBuf, sizeof(minuteBuf));
                GetWindowTextA(hEditThreshold, threshBuf, sizeof(threshBuf));

                int hour = atoi(hourBuf);
                int minute = atoi(minuteBuf);
                int threshold = atoi(threshBuf);

                if (hour < 0) hour = 0;
                if (hour > 23) hour = 23;
                if (minute < 0) minute = 0;
                if (minute > 59) minute = 59;
                if (threshold < 0) threshold = 0;
                if (threshold > 1023) threshold = 1023;

                int melodyIdx = (int)SendMessageA(hComboMelody, CB_GETCURSEL, 0, 0);
                if (melodyIdx == CB_ERR) melodyIdx = 0;
                int melody = melodyIdx + 1;

                BOOL enabled = (SendMessageA(hCheckAlarm, BM_GETCHECK, 0, 0) == BST_CHECKED);

                char cmd[64];
                sprintf(cmd, "ALARM;%02d;%02d;%d;%d;%d\n",
                    hour, minute, enabled ? 1 : 0, melody, threshold);
                SendSerialCommand(cmd);

                sprintf(hourBuf, "%02d", hour);
                sprintf(minuteBuf, "%02d", minute);
                sprintf(threshBuf, "%d", threshold);
                SetWindowTextA(hEditHour, hourBuf);
                SetWindowTextA(hEditMinute, minuteBuf);
                SetWindowTextA(hEditThreshold, threshBuf);
            }
            else if (LOWORD(wParam) == ID_BTN_STOP_ALARM) {
                SendSerialCommand("STOP\n");
            }
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Bufor do podwojnego buforowania
            HDC hdcMem = CreateCompatibleDC(hdc);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP hbmOld = SelectObject(hdcMem, hbmMem);
            
            // Tlo
            HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
            FillRect(hdcMem, &rect, hBrush);
            DeleteObject(hBrush);
            
            // Rysuj dane
            DrawSensorData(hdcMem, &rect);
            
            // Kopiuj na ekran
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == hLabelAlarmStatus) {
                SetTextColor(hdcStatic, sensorData.alarmActive ? COLOR_RED : COLOR_GREEN);
            } else if (hCtrl == hLabelSound) {
                SetTextColor(hdcStatic, COLOR_TEXT);
            } else {
                SetTextColor(hdcStatic, COLOR_ACCENT);
            }
            SetBkColor(hdcStatic, COLOR_BG);
            static HBRUSH hbrBg = NULL;
            if (!hbrBg) hbrBg = CreateSolidBrush(COLOR_BG);
            return (LRESULT)hbrBg;
        }
        
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            CloseSerialPort();
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void DrawSensorData(HDC hdc, RECT* rect) {
    SetBkMode(hdc, TRANSPARENT);
    
    // Czcionki
    HFONT hFontBig = CreateFontA(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT hFontMed = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT hFontSmall = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    
    int y = 130;
    int labelX = 30;
    int valueX = 280;
    char buf[64];
    
    // Temperatura
    SelectObject(hdc, hFontMed);
    SetTextColor(hdc, COLOR_TEXT);
    TextOutA(hdc, labelX, y, "Temperatura:", 12);
    
    // Kolor temperatury
    if (sensorData.temp < 18) SetTextColor(hdc, COLOR_BLUE);
    else if (sensorData.temp > 26) SetTextColor(hdc, COLOR_RED);
    else SetTextColor(hdc, COLOR_GREEN);
    
    sprintf(buf, "%.1f C", sensorData.temp);
    SelectObject(hdc, hFontBig);
    TextOutA(hdc, valueX, y - 5, buf, strlen(buf));
    
    y += 55;
    
    // Wilgotnosc
    SelectObject(hdc, hFontMed);
    SetTextColor(hdc, COLOR_TEXT);
    TextOutA(hdc, labelX, y, "Wilgotnosc:", 11);
    SetTextColor(hdc, COLOR_ACCENT);
    sprintf(buf, "%.1f %%", sensorData.hum);
    SelectObject(hdc, hFontBig);
    TextOutA(hdc, valueX, y - 5, buf, strlen(buf));
    
    y += 55;
    
    // Cisnienie
    SelectObject(hdc, hFontMed);
    SetTextColor(hdc, COLOR_TEXT);
    TextOutA(hdc, labelX, y, "Cisnienie:", 10);
    SetTextColor(hdc, COLOR_ACCENT);
    sprintf(buf, "%d hPa", sensorData.pressure);
    SelectObject(hdc, hFontBig);
    TextOutA(hdc, valueX, y - 5, buf, strlen(buf));
    
    y += 55;
    
    // Swiatlo
    SelectObject(hdc, hFontMed);
    SetTextColor(hdc, COLOR_TEXT);
    TextOutA(hdc, labelX, y, "Swiatlo:", 8);
    SetTextColor(hdc, RGB(255, 200, 0));
    sprintf(buf, "%d %%", sensorData.light);
    SelectObject(hdc, hFontBig);
    TextOutA(hdc, valueX, y - 5, buf, strlen(buf));
    
    y += 45;
    
    // Pasek swiatla
    RECT barRect = {labelX, y, labelX + 380, y + 15};
    HBRUSH hBrushBar = CreateSolidBrush(RGB(60, 60, 70));
    FillRect(hdc, &barRect, hBrushBar);
    DeleteObject(hBrushBar);
    
    int fillWidth = (int)(380.0 * sensorData.light / 100.0);
    RECT fillRect = {labelX, y, labelX + fillWidth, y + 15};
    HBRUSH hBrushFill = CreateSolidBrush(RGB(255, 200, 0));
    FillRect(hdc, &fillRect, hBrushFill);
    DeleteObject(hBrushFill);
    
    y += 40;
    
    // Linia
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    SelectObject(hdc, hPen);
    MoveToEx(hdc, labelX, y, NULL);
    LineTo(hdc, labelX + 380, y);
    DeleteObject(hPen);
    
    y += 20;
    
    // Zegar
    SetTextColor(hdc, COLOR_GREEN);
    SelectObject(hdc, hFontBig);
    sprintf(buf, "%s", sensorData.time);
    TextOutA(hdc, 160, y, buf, strlen(buf));
    
    y += 45;
    
    // Data
    SetTextColor(hdc, COLOR_GRAY);
    SelectObject(hdc, hFontMed);
    sprintf(buf, "%s", sensorData.date);
    TextOutA(hdc, 155, y, buf, strlen(buf));
    
    y += 40;
    
    // RAW
    SetTextColor(hdc, RGB(80, 80, 80));
    SelectObject(hdc, hFontSmall);
    sprintf(buf, "RAW Light: %d", sensorData.lightRaw);
    TextOutA(hdc, 170, y, buf, strlen(buf));
    
    DeleteObject(hFontBig);
    DeleteObject(hFontMed);
    DeleteObject(hFontSmall);
}

void EnumeratePorts(HWND hCombo) {
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    
    char portName[16];
    for (int i = 1; i <= 20; i++) {
        sprintf(portName, "COM%d", i);
        HANDLE h = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
            OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)portName);
        }
    }
    
    if (SendMessageA(hCombo, CB_GETCOUNT, 0, 0) > 0) {
        SendMessageA(hCombo, CB_SETCURSEL, 0, 0);
    }
}

void OpenSerialPort(HWND hwnd) {
    char portPath[32];
    sprintf(portPath, "\\\\.\\%s", comPort);
    
    hSerial = CreateFileA(portPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, 0, NULL);
    
    if (hSerial == INVALID_HANDLE_VALUE) {
        char msg[128];
        sprintf(msg, "Nie mozna otworzyc portu %s", comPort);
        MessageBoxA(hwnd, msg, "Blad", MB_OK | MB_ICONERROR);
        return;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial, &dcb);
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(hSerial, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);
    
    isConnected = TRUE;
    SetWindowTextA(hBtnConnect, "Rozlacz");
    
    char status[64];
    sprintf(status, "Status: Polaczono (%s)", comPort);
    SetWindowTextA(hStatusLabel, status);
}

void CloseSerialPort(void) {
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }
    isConnected = FALSE;
    sensorData.alarmActive = 0;
    sensorData.soundRaw = 0;
    UpdateAlarmUi();
}

void SendSerialCommand(const char* cmd) {
    if (!isConnected || hSerial == INVALID_HANDLE_VALUE || cmd == NULL) return;

    DWORD bytesWritten = 0;
    WriteFile(hSerial, cmd, (DWORD)strlen(cmd), &bytesWritten, NULL);
}

static char serialBuffer[512] = {0};
static int bufferPos = 0;

void ReadSerialData(void) {
    if (hSerial == INVALID_HANDLE_VALUE) return;
    
    DWORD bytesRead;
    char tempBuf[256];
    
    if (ReadFile(hSerial, tempBuf, sizeof(tempBuf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        tempBuf[bytesRead] = '\0';
        
        for (DWORD i = 0; i < bytesRead; i++) {
            if (tempBuf[i] == '\n' || tempBuf[i] == '\r') {
                if (bufferPos > 0) {
                    serialBuffer[bufferPos] = '\0';
                    if (serialBuffer[0] == '{') {
                        ParseJSON(serialBuffer);
                    }
                    bufferPos = 0;
                }
            } else if (bufferPos < sizeof(serialBuffer) - 1) {
                serialBuffer[bufferPos++] = tempBuf[i];
            }
        }
    }
}

void UpdateAlarmUi(void) {
    if (hLabelAlarmStatus) {
        SetWindowTextA(hLabelAlarmStatus, sensorData.alarmActive ? "Alarm: AKTYWNY" : "Alarm: OFF");
    }

    if (hLabelSound) {
        char buf[64];
        sprintf(buf, "Dzwiek: %d", sensorData.soundRaw);
        SetWindowTextA(hLabelSound, buf);
    }
}

void ParseJSON(const char* json) {
    // Prosty parser JSON
    char* ptr;
    
    ptr = strstr(json, "\"temp\":");
    if (ptr) sensorData.temp = (float)atof(ptr + 7);
    
    ptr = strstr(json, "\"hum\":");
    if (ptr) sensorData.hum = (float)atof(ptr + 6);
    
    ptr = strstr(json, "\"pressure\":");
    if (ptr) sensorData.pressure = atoi(ptr + 11);
    
    ptr = strstr(json, "\"light\":");
    if (ptr) sensorData.light = atoi(ptr + 8);
    
    ptr = strstr(json, "\"lightRaw\":");
    if (ptr) sensorData.lightRaw = atoi(ptr + 11);

    ptr = strstr(json, "\"soundRaw\":");
    if (ptr) sensorData.soundRaw = atoi(ptr + 11);

    ptr = strstr(json, "\"alarmActive\":");
    if (ptr) sensorData.alarmActive = atoi(ptr + 14);
    
    ptr = strstr(json, "\"time\":\"");
    if (ptr) {
        ptr += 8;
        int i = 0;
        while (*ptr && *ptr != '"' && i < 15) {
            sensorData.time[i++] = *ptr++;
        }
        sensorData.time[i] = '\0';
    }
    
    ptr = strstr(json, "\"date\":\"");
    if (ptr) {
        ptr += 8;
        int i = 0;
        while (*ptr && *ptr != '"' && i < 15) {
            sensorData.date[i++] = *ptr++;
        }
        sensorData.date[i] = '\0';
    }
}

/*
 * Created by : Jakub Aposte, Alexander Buczek, Miłosz Dobrowolski 
 * Date: 29.12.2025
 * Version: 1.0.0
 * Description: Desktop app for displaying data from Arduino Grove Beginner Kit
 * License: MIT
 * Copyright (c) 2025 Jakub Aposte, Alexander Buczek, Miłosz Dobrowolski
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 */
