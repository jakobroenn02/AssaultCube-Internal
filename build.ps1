# Build script for Assault Cube Trainer Project
# Builds both the Go loader (32-bit) and C++ trainer DLL

param(
    [switch]$SkipGo,
    [switch]$SkipCpp,
    [switch]$Clean
)

Write-Host "===============================================" -ForegroundColor Cyan
Write-Host "  Assault Cube Trainer - Build Script        " -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan
Write-Host ""

# Clean build artifacts if requested
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path "go-project-x86.exe") { Remove-Item "go-project-x86.exe" -Force }
    if (Test-Path "trainer\build") { Remove-Item "trainer\build" -Recurse -Force }
    if (Test-Path "trainer\actrainer.dll") { Remove-Item "trainer\actrainer.dll" -Force }
    Write-Host "[OK] Clean complete`n" -ForegroundColor Green
}

$buildSuccess = $true

# Build Go Loader (32-bit)
if (-not $SkipGo) {
    Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
    Write-Host "Building Go Loader (32-bit)..." -ForegroundColor Cyan
    Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
    
    $env:GOARCH = '386'
    $env:CGO_ENABLED = '0'
    
    go build -o go-project-x86.exe -ldflags="-s -w" .
    
    if ($LASTEXITCODE -eq 0) {
        $fileSize = (Get-Item "go-project-x86.exe").Length / 1MB
        Write-Host "[OK] Go Loader built successfully!" -ForegroundColor Green
        Write-Host "  Output: go-project-x86.exe ($([math]::Round($fileSize, 2)) MB)" -ForegroundColor Gray
        Write-Host ""
    } else {
        Write-Host "[FAIL] Go Loader build failed!" -ForegroundColor Red
        $buildSuccess = $false
    }
}

# Build C++ Trainer DLL
if (-not $SkipCpp) {
    Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
    Write-Host "Building C++ Trainer DLL..." -ForegroundColor Cyan
    Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
    
    Push-Location trainer
    
    # Run the C++ build script
    & .\build_msvc.ps1
    
    if ($LASTEXITCODE -eq 0 -and (Test-Path "actrainer.dll")) {
        $fileSize = (Get-Item "actrainer.dll").Length / 1KB
        Write-Host "[OK] C++ Trainer DLL built successfully!" -ForegroundColor Green
        Write-Host "  Output: trainer\actrainer.dll ($([math]::Round($fileSize, 2)) KB)" -ForegroundColor Gray
        Write-Host ""
    } else {
        Write-Host "[FAIL] C++ Trainer DLL build failed!" -ForegroundColor Red
        $buildSuccess = $false
    }
    
    Pop-Location
}

# Summary
Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
if ($buildSuccess) {
    Write-Host "[OK] Build Complete!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run the loader with:" -ForegroundColor Yellow
    Write-Host "  .\go-project-x86.exe" -ForegroundColor White
} else {
    Write-Host "[FAIL] Build Failed! Check errors above." -ForegroundColor Red
    exit 1
}
Write-Host "-----------------------------------------------" -ForegroundColor DarkGray
