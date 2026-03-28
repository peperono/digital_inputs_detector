@echo off
C:\msys64\msys2_shell.cmd -mingw64 -no-start -defterm -here -c "cd '%~dp0' && bash build.sh && echo && echo Ejecutando app... && echo && ./build/app.exe"
