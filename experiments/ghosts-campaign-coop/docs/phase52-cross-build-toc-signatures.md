# Phase 52 — Cross-build normalized TOC-region comparison

## Purpose

Campaign and Multiplayer are separate PPU builds. Absolute TOC addresses can differ even when corresponding functions use structurally similar data regions. Comparing raw addresses would therefore produce misleading differences.

## Changes

- For each matching function-fingerprint group, the comparer builds normalized TOC-region signatures from block status, block name, permissions, offset within the block, and loaded/initialized/artificial/external/volatile flags.
- Absolute TOC addresses are deliberately excluded from the normalized signature.
- The comparison reports MATCHED_REGION_SIGNATURES, PARTIAL_REGION_OVERLAP, DIFFERENT_REGION_SIGNATURES, NO_TOC_REGION_EVIDENCE, or NOT_RECORDED.
- Summary counters report matching fingerprint groups with equal, partial, different, or missing TOC-region signatures.
- Legacy inventories without Phase 50/51 region metadata remain supported and are marked NOT_RECORDED.

## Validation boundary

Synthetic Campaign/Multiplayer fixtures validate equal signatures despite different absolute addresses, different relative offsets, and missing legacy metadata. A matching region signature is only static layout evidence. It does not prove equal TOC contents, identical relocation state, ABI compatibility, or safe cross-build function calls.
