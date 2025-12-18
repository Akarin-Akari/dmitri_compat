@echo off
set PATH=C:\mingw32\mingw32\bin;%PATH%
cd /d %~dp0

echo [1/8] MinHook buffer...
gcc -O2 -m32 -Iinclude -Iexternal\minhook\include -c external\minhook\src\buffer.c -o build_32bit\obj\minhook\buffer.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [2/8] MinHook hook...
gcc -O2 -m32 -Iinclude -Iexternal\minhook\include -c external\minhook\src\hook.c -o build_32bit\obj\minhook\hook.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [3/8] MinHook trampoline...
gcc -O2 -m32 -Iinclude -Iexternal\minhook\include -c external\minhook\src\trampoline.c -o build_32bit\obj\minhook\trampoline.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [4/8] MinHook hde32...
gcc -O2 -m32 -Iinclude -Iexternal\minhook\include -c external\minhook\src\hde\hde32.c -o build_32bit\obj\minhook\hde32.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [5/8] logger...
g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal\minhook\include -c src\logger.cpp -o build_32bit\obj\logger.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [6/8] config...
g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal\minhook\include -c src\config.cpp -o build_32bit\obj\config.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [7/8] d3d11_hooks...
g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal\minhook\include -c src\hooks\d3d11_hooks.cpp -o build_32bit\obj\d3d11_hooks.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [8/8] main_debug...
g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal\minhook\include -c src\main_debug.cpp -o build_32bit\obj\main_debug.o
if errorlevel 1 (echo FAILED & pause & exit /b 1)

echo [9/9] Linking...
g++ -m32 -shared -o build_32bit\bin\dmitri_compat.dll build_32bit\obj\minhook\buffer.o build_32bit\obj\minhook\hook.o build_32bit\obj\minhook\trampoline.o build_32bit\obj\minhook\hde32.o build_32bit\obj\logger.o build_32bit\obj\config.o build_32bit\obj\d3d11_hooks.o build_32bit\obj\main_debug.o -lkernel32 -luser32 -ld3d11 -ldxgi -static-libgcc -static-libstdc++
if errorlevel 1 (echo FAILED & pause & exit /b 1)

copy /Y config\config.ini build_32bit\bin\config\config.ini

echo.
echo SUCCESS!
dir build_32bit\bin\dmitri_compat.dll
