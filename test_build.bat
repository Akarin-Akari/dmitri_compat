@echo off
REM 构建后测试脚本

echo =========================================
echo DmitriCompat Post-Build Test
echo =========================================
echo.

cd build\bin

REM 重命名 DLL（移除 lib 前缀）
if exist libdmitri_compat.dll (
    copy /Y libdmitri_compat.dll dmitri_compat.dll
    echo [1/3] Renamed DLL: libdmitri_compat.dll -^> dmitri_compat.dll
) else (
    echo ERROR: libdmitri_compat.dll not found!
    pause
    exit /b 1
)

REM 检查文件
echo.
echo [2/3] Generated files:
dir /B *.dll
echo.
dir /B config\*.ini
echo.

REM 显示 DLL 信息
echo [3/3] DLL Information:
powershell -Command "Get-Item dmitri_compat.dll | Select-Object Name, Length, @{Name='SizeMB';Expression={[math]::Round($_.Length/1MB,2)}}"
echo.

cd ..\..

echo =========================================
echo Build Test Complete!
echo =========================================
echo.
echo Next step: Inject DLL into player
echo.
echo Method 1: Using Python injector
echo   python injector.py PotPlayerMini64.exe
echo.
echo Method 2: Using Process Hacker
echo   1. Open Process Hacker (run as admin)
echo   2. Find your player process
echo   3. Right-click -^> Inject DLL
echo   4. Select: build\bin\dmitri_compat.dll
echo.
pause
