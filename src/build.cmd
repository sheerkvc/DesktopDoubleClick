@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Open the x64 Native Tools Command Prompt for Visual Studio first.
  pause
  exit /b 1
)
rc /nologo /fo app.res app.rc
if errorlevel 1 exit /b 1
cl /nologo /O2 /W4 /EHsc /MT /utf-8 main.cpp app.res /Fe:..\DesktopDoubleClick.exe /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT user32.lib shell32.lib ole32.lib oleaut32.lib oleacc.lib uuid.lib advapi32.lib psapi.lib
if errorlevel 1 exit /b 1
echo Built ..\DesktopDoubleClick.exe
endlocal
