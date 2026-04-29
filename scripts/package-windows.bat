@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PACKAGE_PS1=%SCRIPT_DIR%package-windows.ps1"

if not exist "%PACKAGE_PS1%" (
    set "PACKAGE_PS1=%SCRIPT_DIR%scripts\package-windows.ps1"
)
if not exist "%PACKAGE_PS1%" (
    set "PACKAGE_PS1=%CD%\scripts\package-windows.ps1"
)

if not exist "%PACKAGE_PS1%" (
    echo package-windows.ps1 was not found.
    echo Run this BAT from the repository root, scripts directory, or CMake build directory.
    pause
    exit /b 1
)

where pwsh.exe >nul 2>nul
if errorlevel 1 (
    where powershell.exe >nul 2>nul
    if errorlevel 1 (
        echo Neither pwsh.exe nor powershell.exe was found in PATH.
        pause
        exit /b 1
    ) else (
        set "PS_EXE=powershell.exe"
    )
) else (
    set "PS_EXE=pwsh.exe"
)

if "%~1"=="" (
    echo Running portable package flow...
    "%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%PACKAGE_PS1%" -Mode Portable -Clean
) else (
    echo Running package flow with custom arguments...
    "%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%PACKAGE_PS1%" %*
)

set "PACKAGE_EXIT_CODE=%ERRORLEVEL%"
if not "%PACKAGE_EXIT_CODE%"=="0" (
    echo Package flow failed. Exit code: %PACKAGE_EXIT_CODE%
) else (
    echo Package flow finished.
)

pause
exit /b %PACKAGE_EXIT_CODE%
