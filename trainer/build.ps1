# Assault Cube Trainer Build Script
# Requires CMake and Visual Studio C++ tools

Write-Host "=== Building Assault Cube Trainer ===" -ForegroundColor Cyan

# Check if CMake is installed
$cmakeVersion = cmake --version 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake not found. Please install CMake." -ForegroundColor Red
    exit 1
}

Write-Host "CMake found: $($cmakeVersion[0])" -ForegroundColor Green

# Create build directory if it doesn't exist
if (-Not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Navigate to build directory
Set-Location build

# Configure
Write-Host "`nConfiguring project..." -ForegroundColor Yellow
cmake .. -G "Visual Studio 17 2022" -A Win32

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host "If you don't have Visual Studio 2022, try:" -ForegroundColor Yellow
    Write-Host "  cmake .. -G `"Visual Studio 16 2019`" -A Win32" -ForegroundColor Yellow
    Write-Host "Or see available generators with: cmake -G" -ForegroundColor Yellow
    Set-Location ..
    exit 1
}

# Build
Write-Host "`nBuilding project..." -ForegroundColor Yellow
cmake --build . --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "DLL location: $PWD\lib\Release\actrainer.dll" -ForegroundColor Cyan
    Write-Host "Also copied to: $(Split-Path -Parent $PWD)\actrainer.dll" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed!" -ForegroundColor Red
}

Set-Location ..
