# Simple MSVC build script for Assault Cube Trainer
Write-Host "=== Building Assault Cube Trainer (MSVC) ===" -ForegroundColor Cyan

# Find Visual Studio installation
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -property installationPath
    if ($vsPath) {
        Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green
        
        # Import VS environment
        $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars32.bat"
        if (Test-Path $vcvars) {
            Write-Host "Setting up MSVC environment..." -ForegroundColor Yellow
            
            # Compile the trainer
            Write-Host "`nCompiling..." -ForegroundColor Yellow
            
            # Create output directory
            New-Item -ItemType Directory -Force -Path ".\build" | Out-Null
            
            # Run vcvars and compile in one command
            # /MT = Static runtime linking (no external DLL dependencies)
            cmd /c "`"$vcvars`" && cd build && cl.exe /LD /MT /std:c++17 /EHsc /I..\include ..\src\dllmain.cpp ..\src\trainer.cpp ..\src\pch.cpp psapi.lib user32.lib /link /OUT:actrainer.dll" 2>&1
            
            if (Test-Path ".\build\actrainer.dll") {
                Write-Host "`nBuild successful!" -ForegroundColor Green
                Write-Host "DLL location: $(Resolve-Path .\build\actrainer.dll)" -ForegroundColor Green
                
                # Copy to trainer root
                Copy-Item ".\build\actrainer.dll" ".\actrainer.dll" -Force
                Write-Host "Also copied to: $(Resolve-Path .\actrainer.dll)" -ForegroundColor Green
            } else {
                Write-Host "`nBuild failed! Check the output above for errors." -ForegroundColor Red
                exit 1
            }
        } else {
            Write-Host "Could not find vcvars32.bat" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "Visual Studio not found via vswhere" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "vswhere.exe not found. Is Visual Studio installed?" -ForegroundColor Red
    Write-Host "Download Visual Studio Community from: https://visualstudio.microsoft.com/" -ForegroundColor Yellow
    exit 1
}
