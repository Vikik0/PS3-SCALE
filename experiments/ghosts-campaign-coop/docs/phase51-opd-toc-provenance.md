# Phase 51 — TOC memory-block provenance and initialization

## Problem

A TOC address can be mapped in Ghidra without representing a normal loaded runtime region. Ghidra can contain uninitialized, artificial or external blocks, and those must not be treated as ordinary process data.

## Changes

- Each mapped TOC region now records MemoryBlock.isLoaded(), isInitialized(), isArtificial(), isExternalBlock() and isVolatile().
- The flags are serialized as JSON booleans. For ZERO and UNMAPPED TOC entries, the flags must be null because no containing block was found.
- The comparer strictly validates the flag types and rejects mapped records that omit provenance.
- Candidate summaries now count TOC regions backed by loaded/initialized blocks and separately flag artificial or external blocks.

## Validation boundary

The host tests use synthetic JSON-in-CSV fixtures and source-contract assertions. These flags describe the imported Ghidra memory model. They do not prove that the corresponding region exists at the same address at runtime, that the TOC is semantically correct, or that a function can safely be called from Campaign.
