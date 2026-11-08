# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#61 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42482158416)
- Source commit: `94deb82f4097dfe5b2a6fd7d1345736dc8462642`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42481903926/artifacts/13617318299)
- Artifact ID: `13617497295`
- Artifact ZIP SHA-256: `3e182971721a8e1c4e383d8bb03456cf0be0e8748623ae1fb1ed14af812c3976`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,267,963 bytes | `9889bc52efe0b0d8d9eb48a12bab726c810f48ce0f1799907a5ed122506da7b7` |
| `ps3scale.self` | 103,888 bytes | `f3e6b4fc14ea063544f93a465a6a39543671d64f3b0350a31e2331ba5466142a` |
| `ps3scale.pkg` | 112,656 bytes | `c6e2cfd1354f4f8e8cef2c0c33d5f4c80958836a77e5ca3adbf9f15ce7b6f5ca` |
| `ps3scale.gnpdrm.pkg` | 112,656 bytes | `4449df80aa8763fdc330db2668b70b2ef3e54f861583a10e45a8f1cff38272f6` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Declared total size matches actual size: 112,656 bytes
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The finalized package passed structural checks. This is not a substitute for installing and launching it on the physical CECH-2504A.

## Tests and current features
- Host-side SFO parser tests: PASS
- Host-side scanner tests, including local cover discovery: PASS
- Host-side ISO metadata tests: PASS
- Host-side PS2 CONFIG builder tests: PASS
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully
- Main menu: Scan Games, Settings, About
- Separate PS3 and PS2 scan views
- PS3 system on-screen keyboard title search
- Game details page with identity and source path
- Local ICON0.PNG discovery and YA2D rendering for folder/installed titles
- PS2 CONFIG preview only; no sidecar file is written or applied
- Startup MP3 playback and runtime graphics plugin are not yet implemented

## Remaining validation
1. Install the manager package from USB through Package Manager.
2. Confirm the app launches and exits to XMB.
3. Test PS3/PS2 library switching and SELECT search using the controller.
4. Confirm local covers render correctly on the CECH-2504A.
5. Test PS2 CONFIG preview; confirm it does not write any game file.
6. Confirm no original game files or PARAM.SFO files changed.
7. Capture console screenshots and record actual results.
