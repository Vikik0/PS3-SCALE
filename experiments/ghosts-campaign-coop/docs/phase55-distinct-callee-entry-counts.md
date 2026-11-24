# Phase 55 — Count distinct callee entries within fingerprint groups

## Problem

Phase 54 counted at most one edge per caller function and callee fingerprint by using a set of target fingerprints. That still collapsed two distinct resolved callee function entries when their code fingerprints happened to be identical.

## Changes

- Count each distinct resolved callee entry reached by a caller function.
- Aggregate those edges by callee fingerprint for cross-build comparison.
- Continue deduplicating repeated references to the same callee entry within one caller function.
- Preserve separate entries that share a fingerprint, so their multiplicity remains visible.

## Validation

A synthetic inventory has one Campaign caller referencing two distinct target entries with the same fingerprint and duplicate references to one of those entries. Multiplayer has one corresponding target entry referenced twice. The comparison must report 2 versus 1 edges, not 3 versus 2.

This remains a static graph abstraction. It counts distinct resolved function-entry relationships, not instruction call frequency or runtime execution.
