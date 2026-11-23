# Phase 38 — Separate real call references from data XREFs

## Problem found

The previous comparison treated any string reference with a containing function as a function XREF. That is not enough to call it an executable call site: command-registration tables and ordinary data references can live inside a function body. Those rows could inflate the Multiplayer-only lead list.

## Changes

- The Ghidra exporter now writes is_call and is_data using RefType.isCall() and RefType.isData().
- The XREF exporter writes CSV as UTF-8, avoiding dependence on the Windows/Ghidra host default encoding. Phase 39 applies the same encoding to the function-inventory exporter and writes false/false flags for strings with no references.
- The Python reader strictly validates the two boolean columns and rejects conflicting call+data flags.
- Caller fingerprints are built from explicit call references only. Phase 40 further distinguishes direct calls from computed calls, which may represent indirect call flow.
- Multiplayer-only call-path leads are created only when Multiplayer has a call reference and Campaign does not have the same call/caller fingerprint. Data-only references remain visible but never become call-path leads.
- The PPU manifest records total, direct-call, computed-call and data XREF row counts per build. The combined capture summary carries the same counts.

## Validation

Twelve synthetic comparer tests cover call/data classification, false-positive suppression, missing callers, malformed flags, address normalization, duplicate rows and input hashes. The existing Ghidra runner and combined capture tests also verify the call counts are carried into their manifests and summary.

CI host tests do not execute Ghidra. A real decrypted Campaign/Multiplayer TU 1.16 pair is still required to produce actual call XREFs. No Player 2 initializer, hook, or playable build is claimed.
