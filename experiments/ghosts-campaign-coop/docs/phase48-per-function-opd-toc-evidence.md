# Phase 48 — Per-function OPD descriptor and TOC evidence

## Purpose

The earlier OPD stages reported aggregate TOC values but did not associate each function entry with the descriptor(s) that point to it. That association matters for later native-call research because a PPU function descriptor carries both a code pointer and a TOC pointer.

## Changes

- The Ghidra function exporter accepts the validated OPD address/size when OPD seeding is enabled.
- It scans the same descriptor table after the seeder has created functions.
- Each inventory row now includes OPD descriptor addresses, unique TOC mappings with MAPPED/UNMAPPED/ZERO status, and descriptor count for that function.
- The Python inventory reader validates the optional columns and remains backward-compatible with legacy CSVs that have no OPD metadata.
- Multiplayer-only priority candidates now expose their OPD/TOC evidence and a cautious status: no mapping, unmapped TOC present, zero TOC present, or mapped TOC static-only.
- Summary counters report how many Campaign and Multiplayer functions have OPD descriptors and how many priority candidates have unmapped TOCs.

## Validation boundary

Synthetic CSV fixtures and mocked Ghidra commands validate the data path. A mapped TOC only means the address lies in Ghidra memory; it does not prove the TOC is correct for a call, nor that the function can safely run in Campaign.
