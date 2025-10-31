# Simple Trainer DLL Build Script
$ErrorActionPreference = "Stop"

Write-Host "Building C++ Trainer DLL..." -ForegroundColor Yellow

# Find Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio not found"
}

$vsPath = & $vswhere -latest -property installationPath
if (-not $vsPath) {
    throw "Could not find Visual Studio installation path"
}

$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars32.bat"
if (-not (Test-Path $vcvars)) {
    throw "Could not find vcvars32.bat"
}

Write-Host "Using Visual Studio at: $vsPath" -ForegroundColor Gray

# Change to trainer directory
Push-Location "trainer"

try {
    # Create build directory
    New-Item -ItemType Directory -Force -Path ".\build" | Out-Null

    # Build command
    $buildCmd = "`"$vcvars`" && cd build && cl.exe /LD /MT /std:c++17 /EHsc /I..\include /I..\third_party\imgui /I..\third_party\minhook\include ..\src\dllmain.cpp ..\src\trainer.cpp ..\src\aimbot.cpp ..\src\ui.cpp ..\src\gl_hook.cpp ..\src\cursor_hook.cpp ..\src\render_utils.cpp ..\src\pch.cpp ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\imgui_impl_opengl3.cpp ..\third_party\imgui\imgui_impl_win32.cpp ..\third_party\minhook\src\buffer.c ..\third_party\minhook\src\hook.c ..\third_party\minhook\src\trampoline.c ..\third_party\minhook\src\hde\hde32.c psapi.lib user32.lib gdi32.lib opengl32.lib /link /OUT:actrainer.dll"

    Write-Host "Compiling..." -ForegroundColor Gray
    cmd /c $buildCmd

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    # Copy DLL to trainer directory
    if (Test-Path "build\actrainer.dll") {
        Copy-Item "build\actrainer.dll" "actrainer.dll" -Force
        Write-Host "Build successful!" -ForegroundColor Green
        $dllSize = (Get-Item "actrainer.dll").Length
        Write-Host "DLL size: $([math]::Round($dllSize/1KB, 2)) KB" -ForegroundColor Gray
    } else {
        throw "DLL was not created"
    }

} finally {
    Pop-Location
}

Write-Host "Done!" -ForegroundColor Green
