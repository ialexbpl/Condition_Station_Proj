@echo off
echo Kompilacja Condition Station...
echo.

where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo BLAD: Nie znaleziono GCC!
    echo.
    echo Zainstaluj MinGW-w64:
    echo https://www.mingw-w64.org/downloads/
    echo.
    echo Lub uzyj MSYS2:
    echo https://www.msys2.org/
    echo.
    pause
    exit /b 1
)

gcc condition_station.c -o condition_station.exe -lgdi32 -mwindows

if %errorlevel% equ 0 (
    echo.
    echo Sukces! Utworzono condition_station.exe
    echo.
    echo Uruchamiam aplikacje...
    start condition_station.exe
) else (
    echo.
    echo Blad kompilacji!
)

pause

