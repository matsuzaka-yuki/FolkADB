@echo off
REM Quick launcher for ADB Tool

echo ========================================
echo   ADB Tool - Quick Launcher
echo ========================================
echo.

if not exist build\FolkAdb.exe (
    echo ERROR: build\FolkAdb.exe not found!
    echo.
    echo Please run build.bat to compile the project first.
    echo.
    pause
    exit /b 1
)

echo Starting ADB Tool...
echo.
build\FolkAdb.exe

echo.
echo ADB Tool closed. Press any key to exit...
pause >nul
