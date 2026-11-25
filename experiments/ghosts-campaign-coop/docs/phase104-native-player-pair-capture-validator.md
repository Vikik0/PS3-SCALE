# Phase 104 — native Campaign player-pair capture validator

## Purpose

The remaining blocker is runtime evidence for a second native Campaign Local Client. Existing ELF, pad-port and input-port evidence is insufficient to prove player-entity allocation, controller-to-player mapping or independent camera/HUD ownership.

Added a strict JSON capture validator at `tools/ps3mapi/validate_campaign_player_pair_capture.py` and a fill-in template at `tools/ps3mapi/CAMPAIGN_NATIVE_PLAYER_CAPTURE_TEMPLATE.json`.

## Acceptance rules

A capture must identify BLUS31270 / TU 01.16 / Campaign and contain at least five consecutive stable observations. The output status is deliberately named `PASS_STRUCTURAL_CAPTURE_VALIDATION` and always reports `evidence_authenticity: NOT_AUTHENTICATED_BY_THIS_TOOL`. Every accepted observation must prove:
- split-screen active and a player array containing exactly two entities;
- two distinct, valid entity/gentity/gclient/player-state addresses;
- distinct local-client indices 0 and 1;
- distinct controller ports 0 and 1;
- exactly one primary and one secondary split-screen player;
- distinct per-player camera and HUD state addresses, each owned by the corresponding player entity;
- `level.player` resolves to the primary and `level.playengineeffects` resolves to the secondary.

A disconnect gap followed by five stable observations is reported separately as an observed reconnect/rebind cycle.

## Verification

- Added **16 Python tests** covering valid stable pairs, reconnect cycles, wrong title/TU, malformed timestamps, duplicate identities/ports/pointers, alias swaps and insufficient samples.
- The fixture data is generated in tests and explicitly synthetic; it is not represented as PS3 evidence.

## Boundary

The validator does not collect memory, authenticate capture provenance, patch the game, or infer missing addresses. A PASS means only that the supplied observations satisfy the structural consistency rules. The validator does not authenticate the capture source; synthetic or manually fabricated JSON can also pass. Treat a result as runtime evidence only after independently verifying the source and provenance of every observation. No native second Local Client is claimed by this tool alone.
