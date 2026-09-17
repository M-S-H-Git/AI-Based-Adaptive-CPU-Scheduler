@echo off
title AI-Based Adaptive CPU Scheduler
echo.
echo  ================================================================
echo  ^|  AI-Based Adaptive CPU Scheduler - Build ^& Run Script        ^|
echo  ================================================================
echo.

:: Check if g++ is available
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo  [ERROR] g++ compiler not found!
    echo  Please install MinGW-w64 and add it to your PATH.
    echo  You can install it by running:
    echo    winget install --id BrechtSanders.WinLibs.POSIX.UCRT
    echo.
    pause
    exit /b 1
)

echo  [1/2] Compiling cpu_scheduler.cpp ...
g++ -std=c++17 -Wall -Wextra -O2 -o cpu_scheduler.exe cpu_scheduler.cpp

if %errorlevel% neq 0 (
    echo.
    echo  [ERROR] Compilation failed! Check the errors above.
    echo.
    pause
    exit /b 1
)

echo  [OK]  Compilation successful!
echo.
echo  [2/2] Starting the program...
echo  ================================================================
echo.

cpu_scheduler.exe

echo.
pause
