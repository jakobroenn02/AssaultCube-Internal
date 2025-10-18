# Test DLL Loading
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars32.bat"

cmd /c "`"$vcvars`" && cl.exe test_load.cpp && test_load.exe"
