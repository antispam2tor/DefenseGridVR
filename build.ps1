$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$vs = & (Join-Path $PSScriptRoot 'find-vs.ps1')
if (-not (Test-Path -LiteralPath $vs)) { throw 'Visual Studio C++ tools not found.' }
$script = Join-Path $build 'compile.cmd'
@"
@echo off
call "$vs" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cd /d "$build"
cl /nologo /std:c++17 /EHsc /W4 /MT /DWIN32_LEAN_AND_MEAN /LD "$root\src\proxy.cpp" "$root\src\vr.cpp" "$root\src\visibility.cpp" /link /DEF:"$root\src\proxy.def" /OUT:"$build\d3d9.dll" user32.lib d3dcompiler.lib d3d11.lib dxgi.lib dxguid.lib "$root\vendor\openvr\openvr_api.lib"
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $script -Encoding ASCII
& cmd.exe /c $script
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Copy-Item -LiteralPath "$root\vendor\openvr\openvr_api.dll" -Destination $build
Get-Item -LiteralPath (Join-Path $build 'd3d9.dll') | Select-Object FullName, Length
