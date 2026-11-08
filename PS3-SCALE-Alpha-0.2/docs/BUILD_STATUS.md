# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#15 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42479967093)
- Source commit: `cb0474eaf290d53c0a3984b8689947282f1b27e1`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42479967093/artifacts/13616694376)
- Artifact ID: `13616694376`
- Artifact ZIP SHA-256: `f579147f7896f8f7254f78bb66f6f24e54df4e9c1bb293897fd22013af6a1127`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,173,638 bytes | `4f8932ce5780d49281cc21cff3a42f3fafb1e179a777294f8956b1fd5703a08c` |
| `ps3scale.self` | 92,928 bytes | `07ea5d9db5869fb18aca9ee80558df7d2b88cacdbdc7263cba6b3be499b04ed6` |
| `ps3scale.pkg` | 101,712 bytes | `3758a3044aed9bb5f53bca0e5491446ea1b0fa4cb1da05b1d9f2fae5393b2c7b` |
| `ps3scale.gnpdrm.pkg` | 101,712 bytes | `4a8572b30602af3dcc11831a94c2acc9dfc30b68b6e8a748524e8a2a6f31cea8` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Declared total size matches actual size: 101,712 bytes
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The finalized package passed structural checks. This is not a substitute for installing and launching it on the physical CECH-2504A.

## Tests and current features
- Host-side SFO parser tests: PASS
- Host-side scanner tests: PASS
- Host-side ISO metadata tests: PASS
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully
- PS3 and PS2 library tabs in the UI: built successfully
- Controller L1/R1 library switching: compiled successfully

## Remaining validation
1. Install the manager package from USB through Package Manager.
2. Confirm the app launches and exits to XMB.
3. Confirm L1/R1 switches between PS3 and PS2 libraries.
4. Scan the HDD and USB, then repeat the scan to check for duplicates.
5. Confirm no original game files or PARAM.SFO files changed.
6. Capture console screenshots and record actual results.
