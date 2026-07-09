@echo off
setlocal

where pwsh >nul 2>nul
if errorlevel 1 (
    echo pwsh was not found. Install PowerShell 7.6+ or run build_scripts.ps1 with pwsh.
    exit /b 1
)

pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_scripts.ps1" %*
exit /b %ERRORLEVEL%
