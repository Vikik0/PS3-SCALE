# Phase 25 — Pad Probe capture integrity gate

## Purpose

The previous comparator could accept a log pair after seeing one simultaneous `0x03` port mask, even if the PRX had stopped early or the log was missing its completion marker. That was too weak for a capture that is used to decide whether native engine analysis should proceed.

Phase 25 makes capture completeness and provenance explicit before a pair can be classified as ready for engine review.

## Required evidence per mode

A valid v4 capture must contain:

- The exact `Ghosts Campaign Co-op Pad Probe v4` header.
- `READ_ONLY=1`.
- The expected operator-selected mode label.
- `MODE_SOURCE=manual_mode_file` and `MODE_LABEL_IS_OPERATOR_SELECTED=1`.
- Successful `cellPadGetInfo2` samples at checkpoint indices 0, 5, 10, 15, 20, and 25.
- The final `Pad probe finished.` marker.
- No duplicate sample indices and no sample index outside 0–29.

The PRX intentionally logs periodic checkpoints every five iterations plus any port-mask transition. Therefore, six successful checkpoints plus the final marker are the expected evidence of a complete 30-iteration run; the log is not expected to contain all 30 sample lines.

An error at a required checkpoint, a truncated log, a missing completion marker, a wrong probe version, or invalid mode provenance makes the pair inconclusive. A simultaneous `0x03` sample remains visible in the diagnostic details but cannot override an incomplete-capture status.

## Changes

- Added v4 header, read-only flag, completion-marker, checkpoint, duplicate-index and index-range parsing.
- Added a separate provenance gate for the operator-selected Campaign/Multiplayer mode labels.
- The comparator now reports capture completeness and provenance separately from controller visibility.
- The runtime capture analyzer only reports `CAPTURE_READY_FOR_ENGINE_REVIEW` when both logs pass the complete capture and provenance gates and both modes report a simultaneous `0x03` sample. Its summary JSON exposes capture completeness and provenance as separate booleans.
- Added tests for truncated captures, missing completion markers, duplicate sample indices, failed periodic checkpoints, old probe versions, missing read-only metadata and legacy logs.

## Validation boundary

This phase validates the integrity of the diagnostic evidence. It does not add a game hook, create a local client, map a controller to Player 2, or prove a second camera/HUD. No runtime logs from the user's PS3 were supplied as part of this code change. The next native gate still requires actual Campaign and Multiplayer captures from the same TU 1.16 installation.
