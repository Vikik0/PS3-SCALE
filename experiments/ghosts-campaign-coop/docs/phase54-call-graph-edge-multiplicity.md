# Phase 54 — Preserve call-graph edge multiplicity

## Problem

The cross-build call-graph comparison previously converted all callee fingerprints in a matching caller-fingerprint group into sets. When two Campaign caller-function instances reached the same callee fingerprint but only one Multiplayer instance did, both sets still contained the same single fingerprint and the delta was missed.

## Changes

- Count one edge per distinct resolved callee function entry for each caller-function instance, aggregated by callee fingerprint.
- Preserve the existing rule that repeated call instructions from one function to the same callee fingerprint count as one graph edge.
- Compare counters rather than only unique fingerprint sets.
- Report per-build edge counts, shared and side-only multiplicities, and whether the unique callee sets are equal.
- Add a summary counter for multiplicity-only deltas where the unique callee sets match.

## Validation

Two synthetic inventories cover a 2-to-1 edge multiplicity delta with identical unique callee sets, and a 2-to-2 equal case.

This is static resolved-call-graph evidence. It does not count runtime executions or prove that any callee is safe to reuse in Campaign.
