param([string]$Version = '0.1.0')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+([-.][A-Za-z0-9.-]+)?$') { throw 'Invalid version.' }
$dist = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$stage = Join-Path $dist ('stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\build\d3d9.dll","$PSScriptRoot\build\openvr_api.dll","$PSScriptRoot\README.md","$PSScriptRoot\LICENSE","$PSScriptRoot\THIRD_PARTY_NOTICES.md" -Destination $stage
Copy-Item -LiteralPath "$PSScriptRoot\vendor\openvr\LICENSE" -Destination "$stage\LICENSE-OpenVR.txt"
$zip = Join-Path $dist "DefenseGridVR-v$Version-win32.zip"
Compress-Archive -Path "$stage\*" -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  DefenseGridVR-v$Version-win32.zip" -Encoding ASCII
Get-Item -LiteralPath $zip,"$zip.sha256" | Select-Object Name,Length
