# Phase 15 — Clockwork MapEnts source and trigger identification

## Finding

The supplied `clockwork.ff` contains the Clockwork GSC scripts and a MapEnts asset reference, but the MapEnts entity-string payload is not present as a normal text asset in that FastFile. The MapEnts probe over the expanded FastFile zone found no valid entity-string header.

The PS3 IW6 MapEnts layout used by the map converter is:

- MapEnts header size: `0x80` bytes.
- Entity-string pointer: big-endian 32-bit value at header + `0x04`; `0xFFFFFFFF` is the inline-data marker.
- Entity-string character count: signed big-endian 32-bit value at header + `0x08`.
- Entity string: begins at header + `0x80`, with the final NUL included in the stored character count.
- Spawn count: big-endian 16-bit value at header + `0x70`.

This means the next required source is the PS3 map file `maps/clockwork_interior.d3dbsp` (or an already extracted `maps/clockwork_interior.ents` entity string). The FastFile alone is insufficient to identify the exact trigger classname attached to `drill_spot1` and `drill_spot2`.

## New read-only extractor

Added `tools/ps3_mapents_inspect.py`. It scans a supplied big-endian PS3 map file for plausible inline MapEnts headers, validates the entity string length and terminating NUL, parses numeric entity fields, and reports:

- entity index;
- classname (field ID 97);
- targetname (field ID 642);
- all numeric key/value fields;
- header offset, entity-string offset, spawn count and SHA-256.

It scans byte-aligned offsets rather than assuming 4-byte alignment. If more than one candidate is found, it fails closed and asks for an explicit `--map-ents-offset` rather than silently choosing one.

Example:

```bash
python tools/ps3_mapents_inspect.py maps/clockwork_interior.d3dbsp \
  --output-ents maps/clockwork_interior.ents \
  --output-json clockwork_mapents_report.json
```

The extractor is read-only and does not patch or rewrite the map file. Five synthetic tests cover unaligned headers, field extraction, invalid lengths, non-inline pointers and ambiguous candidates.

## Why this matters to the drill patch

The Stage 2 GSC patch captures the toucher directly from the trigger entity's `trigger` event, so it no longer relies on the trigger's flag callback to return the player. The MapEnts file is still required to verify:

1. the actual entity class and targetname for both drill spots;
2. whether the trigger is a one-shot or repeatable entity;
3. whether additional script fields or trigger filters must be preserved;
4. whether the mission callback expects both local players to touch the trigger.

The next runtime gate is still a live Campaign test of P1/P2 activation, death, retry and shared objective completion. No FastFile rebuild, signing or PKG is claimed by this phase.
