# Phase 71 — Extend native PPU string/XREF targets from actual GSC evidence

## Purpose

Phase 68–70 identified two client handles, a two-slot FOV mapping and client-specific visibility calls in Carrier. Phase 71 extends the existing read-only Ghidra exporter so the exact decrypted TU 1.16 Campaign and Multiplayer PPU ELFs can be searched for these newly observed tokens alongside the existing split-screen builtins.

## New candidate terms

- playerpiggyback
- playengineeffects
- cg_playerFovScale0
- cg_playerFovScale1
- hideonclient
- showonclient
- createclientfontstring

The existing split-screen terms remain enabled, including PlayerCMD_Spawn, SV_InitLocalClient and CL_InitLocalClients.

## Behavior

The exporter records every string hit and its Ghidra references, including explicit call/data/computed classification and containing-function metadata. The comparison tool continues to use only call references for call-path leads. A data or registration reference is not treated as an executable call.

## Validation boundary

CI checks that every candidate term is present in the exporter source. No actual Ghidra run is possible until the exact decrypted TU 1.16 Campaign and Multiplayer PPU ELFs are available. A zero-hit result is not proof that a native initializer is absent; the strings may be stripped, hashed or stored outside initialized memory.
