$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$vs = & (Join-Path $PSScriptRoot 'find-vs.ps1')
@"
@echo off
call "$vs" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cd /d "$build"
cl /nologo /std:c++17 /EHsc /W4 /MT "$root\src\matrix_test.cpp" /Fe:"$build\matrix_test.exe"
if errorlevel 1 exit /b %errorlevel%
"$build\matrix_test.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /MT /DWIN32_LEAN_AND_MEAN "$root\src\visibility_test.cpp" /Fe:"$build\visibility_test.exe"
if errorlevel 1 exit /b %errorlevel%
"$build\visibility_test.exe"
exit /b %errorlevel%
"@ | Set-Content -LiteralPath "$build\test.cmd" -Encoding ASCII
& cmd.exe /c "$build\test.cmd"
if ($LASTEXITCODE -ne 0) { throw "Camera math checks failed: $LASTEXITCODE" }
