# Phase 105 — integrate player-pair validation into TU 1.16 analysis

## Change

The read-only TU 1.16 Campaign/Multiplayer analyzer now accepts an optional `--campaign-player-capture` JSON file and runs it through the Phase 104 validator.

The analyzer:
- validates the capture profile before starting ELF reconstruction;
- writes `campaign_player_pair_validation.json` as part of the same staged/atomic output set;
- records the capture file's SHA-256 in `capture_summary.json`;
- embeds the structural validation result in the summary;
- keeps `player_two_gate.status = NOT_PROVEN` and `capture_source_authenticated = false`, even when the supplied JSON passes structural checks.

This prevents a well-formed but unauthenticated JSON file from upgrading the native Player 2 gate.

## Usage

```bash
python tools/ps3mapi/analyze_tu116_runtime_capture.py \
  --campaign-self <default.self> \
  --campaign-dumps <campaign-dump-dir> \
  --multiplayer-self <default_mp.self> \
  --multiplayer-dumps <multiplayer-dump-dir> \
  --campaign-player-capture <campaign-player-pair.json>
```

## Verification

- Python suite: **561/561 passed**.
- Event Lock C model: **21/21 passed**.
- GSC wrapper parser: **10/10 passed**.
- Mocked GSC loader: **12/12 passed**.
- Session-monitor C model: **14/14 passed**.
- PS3 CEX PRX, both read-only controller probes, GSC compile/decompile and importer link validation passed.
- CI run [#1594](https://github.com/Vikik0/PS3-SCALE/actions/runs/45724340069): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **538,042 bytes**, SHA-256 `69ab6ec463a554ebb653cd18f6fe2da00ffad3b705485e24312088466cb85144`; artifact ID `15074350328`.

## Boundary

No real Campaign player-pair capture was supplied. The analyzer integration is ready to consume one, but the native second Local Client and the four original shared interactions remain unproven.
