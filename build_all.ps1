# ============================================
# Full Project Build Script
# ============================================
# Builds both the C++ trainer DLL and Go loader,
# then launches the application
# ============================================

$ErrorActionPreference = "Stop"

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Building Assault Cube Trainer Project" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if (-not $isAdmin) {
    Write-Host "⚠️  WARNING: Not running as Administrator!" -ForegroundColor Yellow
    Write-Host "   DLL injection will likely fail without admin rights." -ForegroundColor Yellow
    Write-Host ""
}

# Get project root directory
$ProjectRoot = $PSScriptRoot
$TrainerDir = Join-Path $ProjectRoot "trainer"
$BuildSuccess = $true

# ============================================
# Step 1: Build C++ Trainer DLL
# ============================================
Write-Host "[1/2] Building C++ Trainer DLL..." -ForegroundColor Yellow
Write-Host "----------------------------------------------" -ForegroundColor DarkGray

try {
    Push-Location $TrainerDir
    
    # Find Visual Studio
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Visual Studio not found. Install Visual Studio 2022 with C++ Desktop Development workload."
    }
    
    $vsPath = & $vswhere -latest -property installationPath
    if (-not $vsPath) {
        throw "Could not find Visual Studio installation path"
    }
    
    $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars32.bat"
    if (-not (Test-Path $vcvars)) {
        throw "Could not find vcvars32.bat"
    }
    
    Write-Host "  Using Visual Studio at: $vsPath" -ForegroundColor DarkGray
    
    # Create build directory
    New-Item -ItemType Directory -Force -Path ".\build" | Out-Null
    
    # Build the DLL (including ui.cpp for overlay support)
    Write-Host "  Compiling C++ files..." -ForegroundColor DarkGray
    $buildCmd = "`"$vcvars`" && cd build && cl.exe /LD /MT /std:c++17 /EHsc /I..\include ..\src\dllmain.cpp ..\src\trainer.cpp ..\src\ui.cpp ..\src\pch.cpp psapi.lib user32.lib gdi32.lib /link /OUT:actrainer.dll"
    
    cmd /c $buildCmd 2>&1 | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        # Try with verbose output to see the error
        cmd /c $buildCmd
        throw "Compilation failed"
    }
    
    # Verify DLL exists
    $DllPath = Join-Path $TrainerDir "actrainer.dll"
    $BuildDllPath = Join-Path $TrainerDir "build\actrainer.dll"
    
    if (Test-Path $BuildDllPath) {
        Copy-Item $BuildDllPath $DllPath -Force
    }
    
    if (-not (Test-Path $DllPath)) {
        throw "actrainer.dll was not created"
    }
    
    $DllSize = (Get-Item $DllPath).Length
    Write-Host "✓ Trainer DLL built successfully!" -ForegroundColor Green
    Write-Host "  Location: $DllPath" -ForegroundColor DarkGray
    Write-Host "  Size: $([math]::Round($DllSize/1KB, 2)) KB" -ForegroundColor DarkGray
    Write-Host ""
    
} catch {
    Write-Host "✗ Trainer DLL build failed: $_" -ForegroundColor Red
    $BuildSuccess = $false
} finally {
    Pop-Location
}

if (-not $BuildSuccess) {
    Write-Host ""
    Write-Host "Build failed. Exiting..." -ForegroundColor Red
    exit 1
}

# ============================================
# Step 2: Build Go Loader
# ============================================
Write-Host "[2/2] Building Go Loader..." -ForegroundColor Yellow
Write-Host "----------------------------------------------" -ForegroundColor DarkGray

try {
    Push-Location $ProjectRoot
    
    # Clean up any old .syso files that might cause issues
    if (Test-Path "tuiapp.syso") {
        Remove-Item "tuiapp.syso" -Force
    }
    
    # Build the Go executable as 32-bit to match the 32-bit game
    Write-Host "  Compiling Go application (32-bit to match game architecture)..." -ForegroundColor DarkGray
    $env:GOARCH = "386"
    go build -o tuiapp.exe
    
    if ($LASTEXITCODE -ne 0) {
        throw "Go build failed with exit code $LASTEXITCODE"
    }
    
    # Note: The tuiapp.manifest file exists for documentation
    # To use it, run: tuiapp.exe (Windows will check for .manifest sidecar file)
    # Or embed it using a tool like mt.exe after build
    
    if ($LASTEXITCODE -ne 0) {
        throw "Go build failed with exit code $LASTEXITCODE"
    }
    
    # Verify executable exists
    $ExePath = Join-Path $ProjectRoot "tuiapp.exe"
    if (-not (Test-Path $ExePath)) {
        throw "tuiapp.exe was not created"
    }
    
    $ExeSize = (Get-Item $ExePath).Length
    Write-Host "✓ Go Loader built successfully!" -ForegroundColor Green
    Write-Host "  Location: $ExePath" -ForegroundColor DarkGray
    Write-Host "  Size: $([math]::Round($ExeSize/1MB, 2)) MB" -ForegroundColor DarkGray
    Write-Host ""
    
} catch {
    Write-Host "✗ Go Loader build failed: $_" -ForegroundColor Red
    $BuildSuccess = $false
} finally {
    Pop-Location
}

if (-not $BuildSuccess) {
    Write-Host ""
    Write-Host "Build failed. Exiting..." -ForegroundColor Red
    exit 1
}

# ============================================
# Build Complete - Summary
# ============================================
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Build Complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Components built:" -ForegroundColor White
Write-Host "  ✓ C++ Trainer DLL (actrainer.dll)" -ForegroundColor Green
Write-Host "  ✓ Go Loader (tuiapp.exe)" -ForegroundColor Green
Write-Host ""

# ============================================
# Step 3: Launch Application
# ============================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Launching Application..." -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Starting tuiapp.exe..." -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop this script after launch." -ForegroundColor DarkGray
Write-Host ""

Start-Sleep -Seconds 1

try {
    # Launch the application
    & .\tuiapp.exe
    
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
        Write-Host ""
        Write-Host "Application exited with code: $LASTEXITCODE" -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "Application closed successfully." -ForegroundColor Green
    }
    
} catch {
    Write-Host ""
    Write-Host "Error launching application: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Done!" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
