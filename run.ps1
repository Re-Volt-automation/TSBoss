# run.ps1 — TSBoss Windows dev loop: stop -> build -> launch
# Usage:  .\run.ps1
# After any code change, run this. It closes the app (so the .exe isn't
# locked), rebuilds, and relaunches only if the build succeeded.

$ErrorActionPreference = 'Stop'

$BuildDir = 'E:\Projects\TSBoss\build-win'
$Exe      = Join-Path $BuildDir 'TSBoss.exe'
$QtBin    = 'C:\Qt\6.8.3\mingw_64\bin'

# 0. Sanity: is the build configured? (fresh clone won't have a cache yet)
if (-not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
    Write-Host "No build configured at $BuildDir." -ForegroundColor Red
    Write-Host "Run the one-time configure step first:" -ForegroundColor Yellow
    Write-Host '  cmake -S E:/Projects/TSBoss -B E:/Projects/TSBoss/build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Windows/ninja.exe'
    exit 1
}

# 1. Close any running instance (Windows locks a running .exe -> link step fails)
$running = Get-Process TSBoss -ErrorAction SilentlyContinue
if ($running) {
    Write-Host 'Closing running TSBoss...' -ForegroundColor Yellow
    $running | Stop-Process -Force
    $running | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
}

# 2. Build
Write-Host 'Building...' -ForegroundColor Cyan
cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED (exit $LASTEXITCODE) - not launching." -ForegroundColor Red
    exit $LASTEXITCODE
}

# 3. Launch (Qt DLLs must be on PATH or the exe exits with 0xC0000135)
Write-Host 'Launching TSBoss...' -ForegroundColor Green
$env:PATH = "$QtBin;$env:PATH"
Start-Process -FilePath $Exe
