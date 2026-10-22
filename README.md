# Defense Grid: The Awakening VR Mod

An unofficial, experimental SteamVR mod for **Defense Grid: The Awakening** on Windows. Play with a stereoscopic bird's-eye view, or stand beside towers and watch the battle at ground level.

Tested by the author with the Windows Steam game and VITURE glasses through VertoXR. Other SteamVR hardware has not been verified. This is for the original game, not Defense Grid 2. No game assets are included.

## Download and install

Download the playable ZIP from [Releases](https://github.com/antispam2tor/DefenseGridVR/releases). GitHub's automatic source archives are for developers and do not contain a built mod DLL.

1. Install the game and SteamVR, and confirm your headset works in SteamVR.
2. Close Defense Grid. In Steam, use **Manage > Browse local files** to locate the folder containing `DefenseGrid.exe`.
3. Extract the release ZIP there. `d3d9.dll` and `openvr_api.dll` must sit beside `DefenseGrid.exe`. If a file with either name already exists, identify and back it up first. Do not overwrite an unrelated graphics wrapper; wrapper chaining is not implemented.
4. Start SteamVR and your headset connection software, then launch Defense Grid normally through Steam.
5. Load a level and press **F6** to enable VR.  The view will not be centered at first press **F7** to recenter.

No compiler or custom INI file is required to play. To uninstall, close the game and remove the two DLLs supplied by this release, restoring any files you backed up.

## Controls

| Key | Action |
| --- | --- |
| F6 | Enable or disable VR output (starts disabled) |
| F7 | Recenter head tracking; in bird's-eye view also restore default VR zoom |
| Up / Down | Hold to zoom in / out in bird's-eye view |
| F9 | Hover a detected tower F9 to stand beside it; press again to return overhead |
| Left / Right | Hold to rotate in place in tower view |
| [  ] | Previous / next tower in tower view |


## Build from source

Requires Windows, Visual Studio 2022 or Build Tools with **Desktop development with C++**, an x86-capable toolchain and Windows SDK. The scripts discover the installation through `vswhere` or an existing developer environment.

```powershell
./build.ps1
./test.ps1
./package.ps1
```

Output is `build/d3d9.dll` and `build/openvr_api.dll` (32-bit). Packaging produces `dist/DefenseGridVR-v0.1.0-win32.zip` and a SHA-256 file. Tests cover camera math, zoom, tower selection/cycling/facing memory, and visibility-hook behavior; they do not replace headset testing.

The mod's code is MIT licensed; see [LICENSE](LICENSE). Vendored Valve OpenVR SDK files retain their [own license](vendor/openvr/LICENSE). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project is not affiliated with or endorsed by the game's developers, Valve, or headset vendors.
