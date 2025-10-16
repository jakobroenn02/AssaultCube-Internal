# Quick run script for Assault Cube Trainer
# Builds if needed and launches the loader

Write-Host "🎮 Assault Cube Trainer - Quick Start" -ForegroundColor Cyan
Write-Host ""

# Check if executables exist
$needsBuild = $false

if (-not (Test-Path "go-project-x86.exe")) {
    Write-Host "⚠️  Go loader not found, building..." -ForegroundColor Yellow
    $needsBuild = $true
}

if (-not (Test-Path "trainer\actrainer.dll")) {
    Write-Host "⚠️  Trainer DLL not found, building..." -ForegroundColor Yellow
    $needsBuild = $true
}

if ($needsBuild) {
    Write-Host ""
    & .\build.ps1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed! Cannot start." -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

# Launch the loader
Write-Host "🚀 Starting Assault Cube Trainer..." -ForegroundColor Green
Write-Host ""

& .\go-project-x86.exe
