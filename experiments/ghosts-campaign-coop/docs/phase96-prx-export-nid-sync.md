# Phase 96 — synchronize PRX exports and NIDs

## Finding and change

The PRX assembly export table contained ten read-only GSC wrapper-loader diagnostics that were missing from `exports.yml`. The YAML file is used by PS3DK `nidgen` to verify exports and generate the import stub archive.

Added the ten diagnostics to the YAML manifest and made the test suite compare both files for exact name/NID equality and duplicate names/NIDs.

The first validation exposed that the ten pre-existing NIDs in `exports.S` were not the values computed by `nidgen`. Corrected both manifests to the computed values. Final validation reports **29 exports** with no mismatch.

## Verification

- Python suite: **533/533 passed**.
- `nidgen verify`: **29 exports accepted**.
- PS3 CEX PRX, Campaign/Multiplayer pad probe, DualShock input probe and PRX importer all built/validated.
- CI run [#1571](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719901826): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **536,522 bytes**, SHA-256 `0d62f02f98b2d24549f45cd1cc0ce062c5d4da022b535909787b1922720c73c7`; artifact ID `15072500004`.

## Boundary

These exports expose loader diagnostics only. They do not install or execute a script override.
