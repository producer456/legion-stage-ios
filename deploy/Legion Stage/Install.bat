@echo off
echo ============================================
echo   Legion Stage - Installer
echo ============================================
echo.

:: Check for admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo [1/3] Installing MSVC Runtime...
if exist "%~dp0vc_redist.x64.exe" (
    "%~dp0vc_redist.x64.exe" /install /quiet /norestart
    echo       Done.
) else (
    echo       Skipped (not found).
)

echo [2/3] Creating program folder...
set "INSTALL_DIR=C:\Program Files\Legion Stage"
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
copy /Y "%~dp0Legion Stage.exe" "%INSTALL_DIR%\"
echo       Installed to %INSTALL_DIR%

echo [3/3] Creating desktop shortcut...
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut([Environment]::GetFolderPath('Desktop') + '\Legion Stage.lnk'); $sc.TargetPath = 'C:\Program Files\Legion Stage\Legion Stage.exe'; $sc.WorkingDirectory = 'C:\Program Files\Legion Stage'; $sc.Save()"
echo       Shortcut created on Desktop.

echo.
echo ============================================
echo   Installation complete!
echo   Launch from your Desktop shortcut.
echo ============================================
echo.
pause
