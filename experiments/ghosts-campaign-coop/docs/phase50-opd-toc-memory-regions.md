# Phase 50 — TOC memory-region and permission evidence

## Purpose

A TOC pointer being present in Ghidra memory is not enough to characterize the region it references. The same mapped status could refer to a read-only data block, writable data, executable memory, or an artificial analysis block.

## Changes

- The Ghidra exporter resolves each non-zero TOC pointer to its containing MemoryBlock.
- Per-function CSV now includes a JSON array named opd_toc_region_evidence. Each unique TOC value records MAPPED/UNMAPPED/ZERO status, block name, read/write/execute permissions, and offset from the block start.
- Zero and unmapped pointers cannot claim a block name, permissions or offset.
- The Python comparer validates the JSON shape, address/status consistency, permissions, relative offset and exact agreement with the existing TOC mapping column.
- Multiplayer-only candidate evidence exposes the TOC memory regions and counts candidates with mapped or writable TOC blocks.

## Validation boundary

Synthetic CSV tests validate parsing and consistency checks. The Ghidra API's MemoryBlock permission flags are used as static import metadata. A mapped writable block is not proof that the pointer is the correct PPC TOC, that it is initialized at runtime, or that a Multiplayer function can safely execute inside Campaign.
