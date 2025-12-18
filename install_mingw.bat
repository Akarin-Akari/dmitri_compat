@echo off
REM 快速安装 MinGW-w64 编译器

echo =========================================
echo MinGW-w64 快速安装脚本
echo =========================================
echo.
echo 方式 1: 使用 Chocolatey (推荐)
echo ----------------------------------------
echo 如果你已经安装了 Chocolatey:
echo.
echo   choco install mingw
echo.
echo 安装后重启命令行窗口
echo.
echo.
echo 方式 2: 手动下载
echo ----------------------------------------
echo 1. 访问: https://winlibs.com/
echo 2. 下载: GCC 13.2.0 + MinGW-w64 11.0.0 (UCRT) - x86_64
echo 3. 解压到 C:\mingw64
echo 4. 添加到 PATH: C:\mingw64\bin
echo.
echo.
echo 方式 3: 使用 MSYS2
echo ----------------------------------------
echo 1. 访问: https://www.msys2.org/
echo 2. 下载并安装 MSYS2
echo 3. 打开 MSYS2，运行:
echo    pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
echo 4. 添加到 PATH: C:\msys64\mingw64\bin
echo.
echo =========================================
pause
