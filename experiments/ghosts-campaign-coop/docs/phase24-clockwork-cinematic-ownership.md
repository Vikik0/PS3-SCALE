# Phase 24 — Clockwork cinematic ownership guard

## Scope

Phase 23 identified two mission-level Bink events in the decompiled Clockwork interior script:

- `cinematicingameloopresident("clockwork_vault_scan")`
- `cinematicingame("clockwork_vault_drill")`

These events are shared mission cinematics. Duplicating either call for each local player could start overlapping playback or interfere with split-screen presentation. The source must retain one call site per event until viewport behavior is verified in a live Campaign session.

## Changes

- Extended `clockwork_drill_adapter_audit.py` to locate and count the two exact cinematic call sites across the GSC function inventory.
- Added separate checks requiring exactly one call site for the vault scan and exactly one for the vault drill.
- The audit report now includes the owning function and invocation count for each event.
- Missing or duplicated call sites fail the corresponding check.
- Added unit tests for the valid one-call case, duplicate calls, a missing event, and preservation of exactly one call site through Stage 5/6 reconstruction.
- Added both cinematic calls to the dedicated synthetic Stage 5/6 GSC Tool fixture so compile/decompile validation covers the Bink event symbols.
- Kept the Stage 2 patcher input fixture separate from the Stage 5/6 toolchain fixture. The toolchain fixture is explicitly synthetic and is not retail source.

## Validation boundary

The audit proves only that the decompiled source contains one call site for each named cinematic. It does not prove that the retail Campaign VM executes the calls once, that Bink playback is visible in both viewports, or that audio/video synchronization is correct. Those points require a live two-controller Campaign capture.

The Bink media files and the original Clockwork source remain unchanged. No FastFile was rebuilt and no PKG was created.
