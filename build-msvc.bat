@echo off
setlocal enabledelayedexpansion

:: Build script for MSVC (Visual Studio)
:: Requires Visual Studio with C++ build tools

echo ========================================
echo   ADB Tool - MSVC Build Script
echo ========================================
echo.

:: Set MSVC environment
:: Try to find Visual Studio installation
set "VS2022=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "VS2019=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
set "VS2017=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

if exist "%VS2022%" (
    call "%VS2022%"
) else if exist "%VS2019%" (
    call "%VS2019%"
) else if exist "%VS2017%" (
    call "%VS2017%"
) else (
    echo ERROR: Visual Studio not found!
    echo Please install Visual Studio 2017/2019/2022 with C++ build tools.
    exit /b 1
)

:: Directories
set SRC_DIR=src
set INC_DIR=include
set RES_DIR=resources
set BUILD_DIR=build

:: Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

:: Compile resource files
echo Compiling resources...
rc /fo %BUILD_DIR%\resources.res %RES_DIR%\resources.rc
if errorlevel 1 (
    echo ERROR: Resource compilation failed!
    exit /b 1
)

:: Compile C files
echo Compiling C files...
cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\main.c /Fo%BUILD_DIR%\main.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\utils.c /Fo%BUILD_DIR%\utils.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\adb_wrapper.c /Fo%BUILD_DIR%\adb_wrapper.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\device_manager.c /Fo%BUILD_DIR%\device_manager.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\file_transfer.c /Fo%BUILD_DIR%\file_transfer.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\resource_extractor.c /Fo%BUILD_DIR%\resource_extractor.obj
if errorlevel 1 goto error

cl /nologo /W3 /O2 /DUNICODE /D_UNICODE /I%INC_DIR% /c %SRC_DIR%\cli.c /Fo%BUILD_DIR%\cli.obj
if errorlevel 1 goto error

:: Link
echo Linking...
link /nologo /subsystem:console ^
   /out:%BUILD_DIR%\adbtool.exe ^
   %BUILD_DIR%\main.obj ^
   %BUILD_DIR%\utils.obj ^
   %BUILD_DIR%\adb_wrapper.obj ^
   %BUILD_DIR%\device_manager.obj ^
   %BUILD_DIR%\file_transfer.obj ^
   %BUILD_DIR%\resource_extractor.obj ^
   %BUILD_DIR%\cli.obj ^
   %BUILD_DIR%\resources.res ^
   user32.lib kernel32.lib shell32.lib ole32.lib

if errorlevel 1 goto error

:: Cleanup intermediate files
del %BUILD_DIR%\*.obj >nul 2>&1

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.
echo Output: %BUILD_DIR%\adbtool.exe
echo.

:: Display file size
for %%A in ("%BUILD_DIR%\adbtool.exe") do echo Size: %%~zA bytes
echo.

goto end

:error
echo.
echo ========================================
echo   Build Failed!
echo ========================================
echo.
exit /b 1

:end
endlocal
