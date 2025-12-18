@echo off
REM DmitriCompat 智能构建脚本
REM 自动检测可用的编译器和构建工具

echo =========================================
echo DmitriCompat Smart Build Script
echo =========================================
echo.

REM 检查 CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found! Please install CMake.
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)

REM 检查编译器
set COMPILER_FOUND=0
set GENERATOR=""

REM 检查 CLion 工具链
echo [1/4] Detecting C++ compiler...
if exist "D:\Program Files\JetBrains\CLion 2023.3.4\bin" (
    echo Found: CLion 2023.3.4
    echo Please use CLion to build this project directly.
    echo See BUILD_WITH_CLION.md for instructions.
    echo.
    choice /C YN /M "Do you want to continue with command line build"
    if errorlevel 2 exit /b 0
)

REM 检查 Visual Studio
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found: MSVC (Visual Studio)
    set GENERATOR=-G "NMake Makefiles"
    set COMPILER_FOUND=1
    goto :build
)

REM 检查 MinGW
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found: MinGW GCC
    set GENERATOR=-G "MinGW Makefiles"
    set COMPILER_FOUND=1
    goto :build
)

REM 没有找到编译器
if %COMPILER_FOUND% EQU 0 (
    echo ERROR: No C++ compiler found!
    echo.
    echo Please choose one of the following options:
    echo.
    echo [Option 1] Use CLion (Recommended)
    echo   - Open dmitri_compat folder in CLion
    echo   - CLion will build automatically
    echo   - See BUILD_WITH_CLION.md
    echo.
    echo [Option 2] Install MinGW-w64
    echo   - Run: install_mingw.bat
    echo   - Or visit: https://winlibs.com/
    echo.
    echo [Option 3] Install Visual Studio Build Tools
    echo   - Download: https://aka.ms/vs/17/release/vs_BuildTools.exe
    echo   - Install "Desktop development with C++"
    echo.
    pause
    exit /b 1
)

:build
echo Compiler detected!
echo Using generator: %GENERATOR%
echo.

REM 创建构建目录
if not exist build mkdir build
cd build

echo [2/4] Configuring CMake...
cmake .. %GENERATOR% -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [3/4] Building Release...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [4/4] Build completed!
echo.
echo Output files:
dir /B bin\*.dll 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: DLL not found in bin\ directory
    echo Searching in other locations...
    dir /B /S *.dll | findstr dmitri_compat
)
echo.
echo Location: %cd%\bin
echo.

cd ..

echo =========================================
echo Build successful!
echo =========================================
echo.
echo Next steps:
echo 1. Check build\bin\dmitri_compat.dll
echo 2. Edit build\bin\config\config.ini if needed
echo 3. Use injector.py to inject into your player
echo.
echo Example: python injector.py PotPlayerMini64.exe
echo.
pause
