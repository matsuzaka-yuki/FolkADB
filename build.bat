@echo off
setlocal

echo ========================================
echo   ADB Tool - Quick Build Script
echo ========================================
echo.

:: Create build directory
if not exist build mkdir build

echo Step 1: Compiling resources...
windres resources/resources.rc build/resources.o
if errorlevel 1 goto error

echo Step 2: Compiling C files...
gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/main.c -o build/main.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/utils.c -o build/utils.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/adb_wrapper.c -o build/adb_wrapper.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/fastboot_wrapper.c -o build/fastboot_wrapper.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/device_manager.c -o build/device_manager.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/file_transfer.c -o build/file_transfer.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/fastboot_manager.c -o build/fastboot_manager.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/resource_extractor.c -o build/resource_extractor.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/cli.c -o build/cli.o
if errorlevel 1 goto error

gcc -Wall -O2 -DUNICODE -D_UNICODE -Iinclude -c src/module_installer.c -o build/module_installer.o
if errorlevel 1 goto error

echo Step 3: Linking...
gcc build/main.o build/utils.o build/adb_wrapper.o build/fastboot_wrapper.o build/device_manager.o build/file_transfer.o build/fastboot_manager.o build/resource_extractor.o build/cli.o build/module_installer.o build/resources.o -o build/FolkAdb.exe -mconsole -luser32 -lkernel32 -lshell32 -lole32
if errorlevel 1 goto error

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.
echo Output: build\FolkAdb.exe
for %%A in ("build\FolkAdb.exe") do echo Size: %%~zA bytes
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
