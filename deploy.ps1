# deploy.ps1 — build a fresh SELF-CONTAINED portable copy in .\deploy
#
# Use this only when you want a standalone TSBoss.exe to share or run without
# Qt on PATH. For everyday testing of your own changes, use .\run.ps1 instead
# (it runs build-win directly and is much faster — no DLL bundling).
#
# Closes any running TSBoss first (a running .exe can't be rebuilt/overwritten).

$ErrorActionPreference = 'Stop'

$BuildDir  = 'E:\Projects\TSBoss\build-win'
$BuildExe  = Join-Path $BuildDir 'TSBoss.exe'
$DeployDir = 'E:\Projects\TSBoss\deploy'
$DeployExe = Join-Path $DeployDir 'TSBoss.exe'
$QtBin     = 'C:\Qt\6.8.3\mingw_64\bin'
$WinDeploy = Join-Path $QtBin 'windeployqt6.exe'

# 1. Close any running instance (so the build can relink and deploy exe is free)
$running = Get-Process TSBoss -ErrorAction SilentlyContinue
if ($running) {
    Write-Host 'Closing running TSBoss...' -ForegroundColor Yellow
    $running | Stop-Process -Force
    $running | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
}

# 2. Build the latest
Write-Host 'Building...' -ForegroundColor Cyan
cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED (exit $LASTEXITCODE) - deploy aborted." -ForegroundColor Red
    exit $LASTEXITCODE
}

# 3. Copy the fresh exe into deploy
if (-not (Test-Path $DeployDir)) { New-Item -ItemType Directory -Path $DeployDir | Out-Null }
Write-Host 'Copying latest exe into deploy...' -ForegroundColor Cyan
Copy-Item $BuildExe $DeployExe -Force

# 3b. Bundle Resources (the Educate button opens the interactive T/S report from
# a Resources folder next to the exe). Installer picks it up via deploy\*.
Write-Host 'Copying Resources...' -ForegroundColor Cyan
Copy-Item 'E:\Projects\TSBoss\Resources' (Join-Path $DeployDir 'Resources') -Recurse -Force

# 4. Top up the bundled Qt DLLs/plugins (idempotent — safe to re-run)
Write-Host 'Running windeployqt...' -ForegroundColor Cyan
$env:PATH = "$QtBin;$env:PATH"
# windeployqt emits warnings on stderr (e.g. missing dxcompiler.dll, which this
# app doesn't use). Under ErrorActionPreference=Stop those abort the script, so
# relax it for this call and judge success by the exit code alone.
$ErrorActionPreference = 'Continue'
& $WinDeploy --release --no-translations $DeployExe 2>&1 | ForEach-Object { "$_" }
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) {
    Write-Host "windeployqt FAILED (exit $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Deploy refreshed: $DeployExe" -ForegroundColor Green
