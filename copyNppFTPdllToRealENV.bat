@echo off
setlocal

set "SOURCE=%~dp0_build\Release\NppFTP.dll"
set "TARGET_DIR=C:\Program Files\Notepad++\plugins\NppFTP"
set "TARGET=%TARGET_DIR%\NppFTP.dll"

if not exist "%SOURCE%" (
    echo ERROR: Build output not found:
    echo   %SOURCE%
    goto :failed
)

if not exist "%TARGET_DIR%\" (
    echo ERROR: Notepad++ plugin directory not found:
    echo   %TARGET_DIR%
    goto :failed
)

copy /Y /B "%SOURCE%" "%TARGET%" >nul
if errorlevel 1 (
    echo ERROR: Could not replace NppFTP.dll.
    echo Close Notepad++ and run this batch file as administrator.
    goto :failed
)

fc /B "%SOURCE%" "%TARGET%" >nul
if errorlevel 1 (
    echo ERROR: The copied DLL does not match the build output.
    goto :failed
)

echo NppFTP.dll updated successfully:
echo   %TARGET%
echo.
pause
exit /b 0

:failed
echo.
pause
exit /b 1
