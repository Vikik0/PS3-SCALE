# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#61 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42481903926)
- Source commit: `38e7c0203d1534caff9365336ad965cc8628c7dd`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42481903926/artifacts/13617497295)
- Artifact ID: `13617497295`
- Artifact ZIP SHA-256: `e74e138858729e67344e8bd7be32ad9f2567b71400e877ae04d5b348360d9df0`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,267,963 bytes | `9889bc52efe0b0d8d9eb48a12bab726c810f48ce0f1799907a5ed122506da7b7` |
| `ps3scale.self` | 103,888 bytes | `05d53bc4a54c23d8cdc41d8c968f7ab2683a61334fd8351c82766481807e8de2` |
| `ps3scale.pkg` | 112,656 bytes | `c2d18ae2c1878c70ad94a3fc11013f9b4236be01af870d97b4af4f378ba516ef` |
| `ps3scale.gnpdrm.pkg` | 112,656 bytes | `3404302611866ec713f09924335188f2220d86d75f060ae036ff2411ec4b0e47` |

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
