@echo off
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
cd /d %~dp0
bash build.sh
echo.
echo Ejecutando app...
echo.
build\app.exe
pause
