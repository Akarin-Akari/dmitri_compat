@echo off
echo ======================================================================
echo Building 32-bit DmitriCompat DLL with MinHook
echo ======================================================================
echo.

set MINGW32=C:\mingw32\mingw32\bin
set SRC_DIR=%~dp0src
set MINHOOK_SRC=%~dp0external\minhook\src
set BUILD_DIR=%~dp0build_32bit
set OUTPUT_DIR=%BUILD_DIR%\bin

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%\obj" mkdir "%BUILD_DIR%\obj"
if not exist "%BUILD_DIR%\obj\minhook" mkdir "%BUILD_DIR%\obj\minhook"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%OUTPUT_DIR%\config" mkdir "%OUTPUT_DIR%\config"
if not exist "%OUTPUT_DIR%\logs" mkdir "%OUTPUT_DIR%\logs"

set CFLAGS=-O2 -Wall -m32
set CXXFLAGS=-std=c++17 -O2 -Wall -m32
set INCLUDES=-I"%~dp0include" -I"%~dp0external\minhook\include"

echo [1/9] Compiling MinHook: buffer.c
"%MINGW32%\gcc.exe" %CFLAGS% %INCLUDES% -c "%MINHOOK_SRC%\buffer.c" -o "%BUILD_DIR%\obj\minhook\buffer.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [2/9] Compiling MinHook: hook.c
"%MINGW32%\gcc.exe" %CFLAGS% %INCLUDES% -c "%MINHOOK_SRC%\hook.c" -o "%BUILD_DIR%\obj\minhook\hook.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [3/9] Compiling MinHook: trampoline.c
"%MINGW32%\gcc.exe" %CFLAGS% %INCLUDES% -c "%MINHOOK_SRC%\trampoline.c" -o "%BUILD_DIR%\obj\minhook\trampoline.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [4/9] Compiling MinHook: hde32.c
"%MINGW32%\gcc.exe" %CFLAGS% %INCLUDES% -c "%MINHOOK_SRC%\hde\hde32.c" -o "%BUILD_DIR%\obj\minhook\hde32.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [5/9] Compiling logger.cpp
"%MINGW32%\g++.exe" %CXXFLAGS% %INCLUDES% -c "%SRC_DIR%\logger.cpp" -o "%BUILD_DIR%\obj\logger.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [6/9] Compiling config.cpp
"%MINGW32%\g++.exe" %CXXFLAGS% %INCLUDES% -c "%SRC_DIR%\config.cpp" -o "%BUILD_DIR%\obj\config.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [7/9] Compiling d3d11_hooks.cpp
"%MINGW32%\g++.exe" %CXXFLAGS% %INCLUDES% -c "%SRC_DIR%\hooks\d3d11_hooks.cpp" -o "%BUILD_DIR%\obj\d3d11_hooks.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [8/9] Compiling main_debug.cpp
"%MINGW32%\g++.exe" %CXXFLAGS% %INCLUDES% -c "%SRC_DIR%\main_debug.cpp" -o "%BUILD_DIR%\obj\main_debug.o"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [9/9] Linking dmitri_compat.dll (32-bit)
"%MINGW32%\g++.exe" -m32 -shared -o "%OUTPUT_DIR%\dmitri_compat.dll" ^
    "%BUILD_DIR%\obj\minhook\buffer.o" ^
    "%BUILD_DIR%\obj\minhook\hook.o" ^
    "%BUILD_DIR%\obj\minhook\trampoline.o" ^
    "%BUILD_DIR%\obj\minhook\hde32.o" ^
    "%BUILD_DIR%\obj\logger.o" ^
    "%BUILD_DIR%\obj\config.o" ^
    "%BUILD_DIR%\obj\d3d11_hooks.o" ^
    "%BUILD_DIR%\obj\main_debug.o" ^
    -lkernel32 -luser32 -ld3d11 -ldxgi -static-libgcc -static-libstdc++

if %ERRORLEVEL% NEQ 0 exit /b 1

copy /Y "%~dp0config\config.ini" "%OUTPUT_DIR%\config\config.ini" >nul

echo.
echo ======================================================================
echo SUCCESS! 32-bit DLL built!
echo ======================================================================
dir "%OUTPUT_DIR%\dmitri_compat.dll"
