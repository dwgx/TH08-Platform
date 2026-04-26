@echo off
:: Launch host + peer for local Phase 6 testing.
:: Usage: scripts\run-2p.bat
::    or: scripts\run-2p.bat <path\to\th08.exe>

setlocal
set "REPO_ROOT=%~dp0.."
set "LOADER=%REPO_ROOT%\dll\build\bin\Release\th08_platform_loader.exe"

if "%~1"=="" (
    set "GAME=D:\Project\THGlobal\th08\th08.exe"
) else (
    set "GAME=%~1"
)

if not exist "%LOADER%" (
    echo [run-2p] loader not found: %LOADER%
    echo [run-2p] build it first: cd dll ^&^& cmake -B build -A Win32 ^&^& cmake --build build --config Release
    exit /b 1
)
if not exist "%GAME%" (
    echo [run-2p] th08.exe not found: %GAME%
    echo [run-2p] pass a path:  scripts\run-2p.bat C:\path\to\th08.exe
    exit /b 1
)

echo [run-2p] starting HOST  on listen 7480
start "th08 host" "%LOADER%" "%GAME%" --host

echo [run-2p] waiting 2s for host listener
timeout /t 2 /nobreak >nul

echo [run-2p] starting PEER  to 127.0.0.1:7480
start "th08 peer" "%LOADER%" "%GAME%" --peer 127.0.0.1:7480

echo [run-2p] both launched. Logs at %%LOCALAPPDATA%%\th08_platform\log_pid*.txt
endlocal
