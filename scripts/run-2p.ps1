# Launch host + peer for local Phase 6 testing.
# Usage:
#   .\scripts\run-2p.ps1
#   .\scripts\run-2p.ps1 -Game "C:\path\to\th08.exe"
#   .\scripts\run-2p.ps1 -Kill            # kill all running th08.exe instances
#   .\scripts\run-2p.ps1 -CleanLogs       # rm log_pid*.txt + log.txt

param(
    [string]$Game = "D:\Project\THGlobal\th08\th08.exe",
    [switch]$Kill,
    [switch]$CleanLogs
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Loader   = Join-Path $RepoRoot "dll\build\bin\Release\th08_platform_loader.exe"
$LogDir   = Join-Path $env:LOCALAPPDATA "th08_platform"

if ($Kill) {
    Get-Process -Name th08 -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "[run-2p] killing th08.exe pid=$($_.Id)"
        Stop-Process -Id $_.Id -Force
    }
    return
}

if ($CleanLogs) {
    if (Test-Path $LogDir) {
        Get-ChildItem -Path $LogDir -Filter "log*.txt" -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "[run-2p] rm $($_.FullName)"
            Remove-Item -Path $_.FullName -Force -ErrorAction SilentlyContinue
        }
    }
    return
}

if (-not (Test-Path $Loader)) {
    Write-Host "[run-2p] loader not found: $Loader" -ForegroundColor Red
    Write-Host "[run-2p] build first: cd dll; cmake -B build -A Win32; cmake --build build --config Release"
    exit 1
}
if (-not (Test-Path $Game)) {
    Write-Host "[run-2p] th08.exe not found: $Game" -ForegroundColor Red
    Write-Host "[run-2p] pass a path: .\scripts\run-2p.ps1 -Game C:\path\to\th08.exe"
    exit 1
}

Write-Host "[run-2p] starting HOST on listen 7480"
Start-Process -FilePath $Loader -ArgumentList "`"$Game`"", "--host" | Out-Null

Write-Host "[run-2p] waiting 2s for host listener"
Start-Sleep -Seconds 2

Write-Host "[run-2p] starting PEER to 127.0.0.1:7480"
Start-Process -FilePath $Loader -ArgumentList "`"$Game`"", "--peer", "127.0.0.1:7480" | Out-Null

Write-Host "[run-2p] both launched. Logs at $LogDir\log_pid*.txt"
Write-Host "[run-2p] kill both:    .\scripts\run-2p.ps1 -Kill"
Write-Host "[run-2p] clean logs:   .\scripts\run-2p.ps1 -CleanLogs"
