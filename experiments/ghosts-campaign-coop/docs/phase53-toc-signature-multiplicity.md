# Phase 53 — Preserve TOC-region signature multiplicity

## Problem

Phase 52 compared normalized TOC-region signatures as sets. If two Campaign functions in the same fingerprint group referenced the same region signature but the corresponding Multiplayer group contained only one such function, set comparison collapsed both sides to one item and incorrectly reported a full match.

## Changes

- Normalized region signatures are now retained as sorted lists with duplicates.
- Cross-build comparison uses signature counters (multisets), not sets.
- Shared, Campaign-only and Multiplayer-only signature lists preserve their multiplicity.
- The report includes per-signature Campaign, Multiplayer, shared and side-only counts, plus total signature counts.
- Existing status values and legacy NOT_RECORDED handling remain unchanged.

## Validation

Two synthetic cross-build tests cover a 2-to-1 multiplicity mismatch and a 2-to-2 match with different absolute TOC addresses. Existing equal/partial/different/legacy tests remain in place.

This is static inventory comparison only. Multiplicity agreement does not prove equal TOC contents, runtime relocation, ABI compatibility or safe native calls.
