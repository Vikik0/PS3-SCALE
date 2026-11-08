# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#34 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42480769127)
- Source commit: `b42bbca2783c6306d03bfa74461534613d820a99`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42480769127/artifacts/13618010014)
- Artifact ID: `13618010014`
- Artifact ZIP SHA-256: `55fbc147f77986c4537a266e6108ab2841964bab810c37e12c72a8585025323a`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,173,871 bytes | `62ceb51bbef1daa2754622305ce21721b62993387e70423916f8d40fdcf689f5` |
| `ps3scale.self` | 93,904 bytes | `eb7e6e2bb312d8a818de803822848e71f3c50ddecfcffab79b6f5c8bbc7c2f05` |
| `ps3scale.pkg` | 102,672 bytes | `62d480ac9159f0d7e5aa9ad1497abe98c99910aea96046245356d0500e1b1165` |
| `ps3scale.gnpdrm.pkg` | 102,672 bytes | `a4614ab854f5ce218f5fb59e3ff78b9f567611886868a206be7cf9ec2c819a59` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Declared total size matches actual size: 102,672 bytes
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The finalized package passed structural checks. This is not a substitute for installing and launching it on the physical CECH-2504A.

## Tests and current features
- Host-side SFO parser tests: PASS
- Host-side scanner tests: PASS
- Host-side ISO metadata tests: PASS
- Host-side PS2 CONFIG builder tests: PASS
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully
- Separate PS3 and PS2 library tabs: compiled successfully
- L1/R1 library switching: compiled successfully
- Triangle opens a PS2 ISO CONFIG preview; it does not write or install files.

## Remaining validation
1. Install the manager package from USB through Package Manager.
2. Confirm the app launches and exits to XMB.
3. Confirm L1/R1 switches between PS3 and PS2 libraries.
4. Confirm Triangle displays the CONFIG preview for a PS2 ISO and does not write a file.
5. Scan the HDD and USB, then repeat the scan to check for duplicates.
6. Confirm no original game files or PARAM.SFO files changed.
7. Capture console screenshots and record actual results.
