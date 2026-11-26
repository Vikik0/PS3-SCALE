# Phase 106 — harden Campaign player-pair evidence continuity

## Problem found

The Phase 104 validator treated every rejected observation as a disconnect gap. A bad camera-owner pointer, alias mismatch, or malformed player record could therefore be followed by five valid samples and incorrectly set `auto_rebind_cycle_observed_in_supplied_data=true`.

The validator also counted samples as one stable run even when their timestamps were separated by arbitrarily long periods. Widely separated snapshots are not evidence of a continuously stable pair.

## Changes

Updated `tools/ps3mapi/validate_campaign_player_pair_capture.py`:

- A reconnect/rebind cycle now requires an explicit pair-loss observation: split-screen is observed inactive, or the observed player array contains fewer than two players.
- Structural errors still break a stable run, but no longer imply a disconnect.
- Added a maximum inter-sample interval, default **1,000 ms**. A larger gap breaks the continuity run and is recorded in `continuity_breaks`.
- Added `--max-sample-gap-ms` for an operator who has a documented reason to use a different sampling interval.
- Reports the best observed mapping between local-client index, controller port, entity ID and primary/secondary role. It does not assume controller port order from the role.
- Keeps `evidence_authenticity=NOT_AUTHENTICATED_BY_THIS_TOOL`; structural validation never authenticates the capture source.

## Usage

```powershell
python tools/ps3mapi/validate_campaign_player_pair_capture.py `
  --capture "D:\Ghosts\captures\campaign-player-pair.json" `
  --required-stable-samples 5 `
  --max-sample-gap-ms 1000
```

## Tests added

- A malformed structural sample cannot falsely prove disconnect/reconnect.
- Five snapshots spaced 1.5 seconds apart do not count as a stable run under the default 1-second limit.
- An explicitly raised sample-gap limit is honored.
- Controller-port-to-player mapping is reported even when port order differs from primary/secondary order.
- Invalid sample-gap limits are rejected.

The player-pair validator test file now contains **21 tests**.

## Boundary

This phase improves evidence interpretation only. It does not collect PS3 memory, authenticate the operator, create a second Local Client, patch Campaign GSC, rebuild a FastFile, or prove gameplay. A real BLUS31270 / TU 1.16 Campaign capture is still required before the native Player 2 gate can advance.
