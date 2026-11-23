# Phase 40 — Direct versus computed call-reference ranking

## Purpose

Phase 38 separated call references from data references. Phase 40 adds one more distinction: a Ghidra call reference can be direct or computed. Computed references may represent indirect call flow and should not receive the same investigation priority as a direct call reference.

## Changes

- The Ghidra XREF exporter now records RefType.isComputed() in an is_computed CSV field.
- The comparer classifies direct_call_xref, computed_call_xref, data_xref, computed_non_call_xref, other_xref and strings without references.
- Multiplayer-only direct call leads rank above computed call leads; caller-fingerprint differences rank after both.
- The PPU manifest records direct and computed call counts for Campaign and Multiplayer.
- The combined capture summary exposes the same direct/computed counts, preserving the evidence when the one-command capture flow is used.
- The comparison JSON schema advances to version 3.

## Validation

The new synthetic test verifies that direct-call leads rank ahead of computed-call leads and that each count is reported separately. Existing tests continue to cover data-only false-positive suppression, unknown callers, malformed reference flags, UTF-8 output, and atomic output promotion.

CI does not execute Ghidra. The actual reference distribution can only be known after running the exporter on the exact Campaign and Multiplayer TU 1.16 PPU pair.
