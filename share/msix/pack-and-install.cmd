@echo off
setlocal EnableExtensions
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "SRC=%LOCALAPPDATA%\KeePassXC"
set "STAGE=%ROOT%\share\msix\staging"
set "OUT=%ROOT%\share\msix\out"
set "CERTDIR=%ROOT%\share\msix\certs"
set "MAKEAPPX=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe"
set "SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
set "MSIX=%OUT%\KeePassXC-OSPasskeys.msix"

if not exist "%SRC%\KeePassXC.exe" (
  echo Missing installed app at %SRC%
  exit /b 1
)

rmdir /s /q "%STAGE%" 2>nul
rmdir /s /q "%OUT%" 2>nul
mkdir "%STAGE%"
mkdir "%OUT%"
mkdir "%CERTDIR%" 2>nul

echo [1/5] Staging package payload...
robocopy "%SRC%" "%STAGE%" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
mkdir "%STAGE%\Assets" 2>nul
copy /Y "%ROOT%\share\msix\Assets\*.png" "%STAGE%\Assets\" >nul
copy /Y "%ROOT%\share\msix\Package.appxmanifest" "%STAGE%\AppxManifest.xml" >nul

echo [2/5] Creating self-signed certificate...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\share\msix\make-cert.ps1"
if errorlevel 1 exit /b 1

echo [3/5] Packaging MSIX...
"%MAKEAPPX%" pack /d "%STAGE%" /p "%MSIX%" /o
if errorlevel 1 exit /b 1

echo [4/5] Signing MSIX...
"%SIGNTOOL%" sign /fd SHA256 /a /f "%CERTDIR%\KeePassXC-OSPasskeys.pfx" /p keepassxc "%MSIX%"
if errorlevel 1 exit /b 1

echo [5/5] Installing MSIX...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\share\msix\install-msix.ps1"
exit /b %ERRORLEVEL%
