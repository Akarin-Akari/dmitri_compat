@echo off
C:\mingw32\mingw32in\gcc.exe -O2 -Wall -m32 -c external\minhook\srcuffer.c -o build_32bit\objuffer_test.o
if %ERRORLEVEL% EQU 0 (
  echo SUCCESS
  dir build_32bit\objuffer_test.o
) else (
  echo FAILED with code %ERRORLEVEL%
)
