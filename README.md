# DefenseGridVR

An unofficial, experimental SteamVR mod for **Defense Grid: The Awakening** on Windows. Play with a stereoscopic bird's-eye view, or stand beside towers and watch the battle at ground level.

Tested by the author with the Windows Steam game and VITURE glasses through VertoXR. Other SteamVR hardware has not been verified. This is for the original game, not Defense Grid 2. No game assets are included.

## Download and install

Download the playable ZIP from [Releases](https://github.com/antispam2tor/DefenseGridVR/releases). GitHub's automatic source archives are for developers and do not contain a built mod DLL.

1. Install the game and SteamVR, and confirm your headset works in SteamVR.
2. Close Defense Grid. In Steam, use **Manage > Browse local files** to locate the folder containing `DefenseGrid.exe`.
3. Extract the release ZIP there. `d3d9.dll` and `openvr_api.dll` must sit beside `DefenseGrid.exe`. If a file with either name already exists, identify and back it up first. Do not overwrite an unrelated graphics wrapper; wrapper chaining is not implemented.
4. Start SteamVR and your headset connection software, then launch Defense Grid normally through Steam.
5. Load a level and press **F6** to enable VR. Use the game's normal mouse/keyboard controls for building and gameplay.

No compiler or custom INI file is required to play. To uninstall, close the game and remove the two DLLs supplied by this release, restoring any files you backed up.

## Controls

| Key | Action |
| --- | --- |
| F6 | Enable or disable VR output (starts disabled) |
| F7 | Recenter head tracking; in bird's-eye view also restore default VR zoom |
| Up / Down | Hold to zoom in / out in bird's-eye view |
| F9 | Hover a detected tower to stand beside it; press again to return overhead |
| Left / Right | Hold to rotate in place in tower view |
| [ / ] | Previous / next detected tower, wrapping around |
| F10 | Toggle the visibility fix for comparison; enabled by default |
| F8 | Capture rendering diagnostics |

Each visited tower location remembers its last left/right facing direction. Returning through brackets or F9 restores it. Head tracking recenters on arrival. Facing memory resets when VR restarts or the graphics device resets; it is not stored in game saves. The same location is reused if a tower is rebuilt there. Overhead zoom is preserved when visiting a tower and returning. F7 does not reset the native game's camera pan/zoom.

Without a custom calibration, initial tower facing follows the overhead camera's horizontal direction. Adjust it with Left/Right. The mod does not automatically identify the nearby enemy path.

## Compatibility and limitations

- The disappearance fix was tested with the author's Steam executable: x86 PE timestamp `0x510c67e9`, image size `0xc35000`. It additionally checks code signatures. On an unsupported version the visibility hook skips installation; other VR features may run but enemies/platforms can disappear. Other storefronts/builds have not been verified.
- Visibility expansion bypasses perspective frustum rejection while VR is enabled. It leaves orthographic passes and application-hidden objects alone, but rendering more objects can reduce performance.
- Rendering uses alternating game frames for the two eyes and CPU readback from D3D9 into D3D11. Fast movement can produce temporal stereo mismatch and performance is not equivalent to a native VR game.
- Tower detection is based on rendered model transforms, not an engine tower ID. Coverage of every tower type/upgrade is not guaranteed. Keep the visibility fix enabled for broad cycling coverage. Placement is not collision-tested.
- Gameplay remains mouse/keyboard driven. VR motion-controller interaction is not implemented, and menus/UI are experimental.
- The DLL writes `dg1-steamvr.log`, `dg1-camera.log` and shader diagnostic files into the game folder. These are not required for distribution. Logs may grow during play.

For problems, include the game version, headset/runtime, steps to reproduce, and relevant log excerpts in an [issue](https://github.com/antispam2tor/DefenseGridVR/issues). Check `dg1-steamvr.log` for the visibility-hook installation or signature-mismatch message.

## Build from source

Requires Windows, Visual Studio 2022 or Build Tools with **Desktop development with C++**, an x86-capable toolchain and Windows SDK. The scripts discover the installation through `vswhere` or an existing developer environment.

```powershell
./build.ps1
./test.ps1
./package.ps1
```

Output is `build/d3d9.dll` and `build/openvr_api.dll` (32-bit). Packaging produces `dist/DefenseGridVR-v0.1.0-win32.zip` and a SHA-256 file. Tests cover camera math, zoom, tower selection/cycling/facing memory, and visibility-hook behavior; they do not replace headset testing.

The mod's code is MIT licensed; see [LICENSE](LICENSE). Vendored Valve OpenVR SDK files retain their [own license](vendor/openvr/LICENSE). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project is not affiliated with or endorsed by the game's developers, Valve, or headset vendors.
