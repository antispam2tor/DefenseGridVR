$ErrorActionPreference = 'Stop'
if ($env:VSINSTALLDIR) {
    $candidate = Join-Path $env:VSINSTALLDIR 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $candidate) { return $candidate }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 or Build Tools with Desktop development with C++ and a Windows SDK.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ x86/x64 tools were not found.' }
Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
