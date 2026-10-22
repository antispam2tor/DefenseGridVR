First public experimental release of DefenseGridVR for Defense Grid: The Awakening.

Download **DefenseGridVR-v0.1.0-win32.zip** below; the automatic source-code downloads are for developers.

### Features

- SteamVR stereoscopic bird's-eye view with head tracking.
- Up/Down zoom and F7 reset/recenter.
- F9 tower-side view, Left/Right turning, and bracket-key tower cycling.
- Per-tower facing memory during the VR session.
- Perspective visibility fix for disappearing enemies and platforms on the supported executable.

### Install

Close the game. Extract the ZIP beside `DefenseGrid.exe`, with `d3d9.dll` and `openvr_api.dll` in that same folder. Do not overwrite an unrelated existing DLL. Start SteamVR and your headset connection, launch the game normally, load a level, and press F6. See the included README for all controls and uninstallation.

### Status

Tested with the author's Windows Steam game and VITURE/VertoXR setup. Other headsets and game builds have not been verified. The visibility hook checks executable signatures and disables itself on a mismatch. Rendering uses alternating frames and CPU readback, with performance and temporal-stereo limitations. This is an experimental prerelease, not native VR support.

Source is MIT licensed; Valve's OpenVR license is included separately. No game assets are included. The accompanying `.sha256` file verifies the release ZIP.
