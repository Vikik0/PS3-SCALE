# Phase 107 — bind player-pair capture to the exact Campaign SELF

## Problem

The Phase 105 analyzer hashed the supplied player-pair JSON, but did not check whether that capture declared the same Campaign executable file that the analyzer was currently processing. A stale capture from another copy/build with the same Title ID and TU label could be included in the report.

## Change

Added `tools/ps3mapi/campaign_capture_binding.py` and integrated it into `analyze_tu116_runtime_capture.py`.

When the player-pair JSON contains `campaign_self_sha256`, the analyzer:

1. Computes SHA-256 over the exact Campaign `default.self` input file.
2. Validates that the declared value is a 64-character hexadecimal digest.
3. Rejects a mismatch before starting PT_LOAD/ELF analysis.
4. Reports `PASS_CAMPAIGN_SELF_SHA256_MATCH` when the hashes match.
5. Keeps `capture_source_authenticated=false` and the Player 2 gate at `NOT_PROVEN`.

If the field is absent, the report says `NOT_PROVIDED` and `capture_bound_to_analyzer_self=false`; it does not silently assume a match.

## Preparing the capture JSON

On Windows, calculate the exact file hash from the same Campaign `default.self` that will be passed to the analyzer:

```powershell
(Get-FileHash "D:\Ghosts\TU116\default.self" -Algorithm SHA256).Hash
```

Copy that value into `campaign_self_sha256` in `CAMPAIGN_NATIVE_PLAYER_CAPTURE_TEMPLATE.json`.

The field binds the JSON to the file being analyzed. It does **not** prove who collected the runtime observations, whether the data came from a physical PS3, or whether the observations themselves are truthful.

## Tests

Added five unit tests for the binding helper and two analyzer integration tests for missing and mismatched hashes. A mismatch is tested to fail before the dump analyzer is called.

## Boundary

No real capture has been supplied. This phase improves profile consistency and prevents accidental cross-build evidence reuse; it does not authenticate the operator, prove a second Campaign Local Client, or make the mod playable.
