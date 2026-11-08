# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## GitHub Actions
- Workflow run: [#8 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42479563035)
- Commit: `20b5ef24badbcea53882ee98cb86379eb302958c`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42479563035/artifacts/13616618869)
- Artifact ID: `13616618869`
- Artifact SHA-256: `f7994f8feb972a0b843cf4bdfad500697215925a796fdcf5fa03e55713d1b81e`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,173,601 bytes | `c4a54802d860e26c15ce07a7d9c3c49e82aa24ffae19145faacb3018318558c5` |
| `ps3scale.self` | 92,080 bytes | `a80b5b55294c934b1e4b01bda2bbfa6ef7222ae8fa2094b52853daf7eb2e1f3d` |
| `ps3scale.pkg` | 100,816 bytes | `a62862ff195bc9540a6609f33fab260a530c7d8617ec5731eaad420f3ee009ce` |
| `ps3scale.gnpdrm.pkg` | 100,816 bytes | `862cb81a30eceedb5fce5cc04f8309d60f920b48ba7a9455610857b63d6994f9` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Declared total size matches actual size: 100,816 bytes
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The finalized package passed structural checks. This is not a substitute for installing and launching it on the physical CECH-2504A.

## Remaining validation
1. Install the manager package from USB through Package Manager.
2. Confirm the app launches and exits to XMB.
3. Scan the HDD and USB, then repeat the scan to check for duplicates.
4. Confirm no original game files or PARAM.SFO files changed.
5. Capture console screenshots and record actual results.
