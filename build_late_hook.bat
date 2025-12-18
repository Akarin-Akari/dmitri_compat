@echo off
REM Build Late Hook version of DmitriCompat
REM This version can hook existing D3D11 devices

echo ========================================
echo Building DmitriCompat Late Hook Edition
echo ========================================

cd /d "%~dp0"

REM Check for MinGW
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: g++ not found!
    echo Please add MinGW to PATH
    pause
    exit /b 1
)

echo.
echo Compiling late_hook.cpp...
g++ -c -std=c++17 -O2 -DNDEBUG ^
    -I"include" ^
    -I"external/minhook/include" ^
    src/hooks/late_hook.cpp ^
    -o build/late_hook.o

if %ERRORLEVEL% neq 0 (
    echo FAILED: late_hook.cpp
    pause
    exit /b 1
)
echo OK: late_hook.o

echo.
echo Compiling main_late_hook.cpp...
g++ -c -std=c++17 -O2 -DNDEBUG ^
    -I"include" ^
    src/main_late_hook.cpp ^
    -o build/main_late_hook.o

if %ERRORLEVEL% neq 0 (
    echo FAILED: main_late_hook.cpp
    pause
    exit /b 1
)
echo OK: main_late_hook.o

echo.
echo Compiling logger.cpp...
g++ -c -std=c++17 -O2 -DNDEBUG ^
    -I"include" ^
    src/logger.cpp ^
    -o build/logger.o

if %ERRORLEVEL% neq 0 (
    echo FAILED: logger.cpp
    pause
    exit /b 1
)
echo OK: logger.o

echo.
echo Compiling config.cpp...
g++ -c -std=c++17 -O2 -DNDEBUG ^
    -I"include" ^
    src/config.cpp ^
    -o build/config.o

if %ERRORLEVEL% neq 0 (
    echo FAILED: config.cpp
    pause
    exit /b 1
)
echo OK: config.o

echo.
echo Linking dmitri_late_hook.dll...
g++ -shared -o build/bin/dmitri_late_hook.dll ^
    build/main_late_hook.o ^
    build/late_hook.o ^
    build/logger.o ^
    build/config.o ^
    build/libminhook.a ^
    -ld3d11 -ldxgi ^
    -static-libgcc -static-libstdc++ ^
    -Wl,--enable-stdcall-fixup

if %ERRORLEVEL% neq 0 (
    echo FAILED: Linking
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! Built: build/bin/dmitri_late_hook.dll
echo ========================================
echo.
echo To test:
echo   1. Open PotPlayer with DmitriRender
echo   2. Run: python auto_test.py --dll build\bin\dmitri_late_hook.dll
echo.
pause
