# Phase 49 — OPD function-pointer resolution accounting

## Problem found

Phase 48 associated an OPD descriptor with the Ghidra function that contained its code pointer. That association did not distinguish a pointer equal to the function entry from a pointer located somewhere inside a function body. It also silently omitted executable pointers that Ghidra could not associate with any function.

## Changes

- The exporter classifies each executable OPD code pointer as an exact function-entry match, a containing-function-only match, or unresolved.
- Per-function CSV rows now carry exact-entry and containing-function descriptor counts. Their sum must equal the row's descriptor count.
- A structured JSON resolution summary accounts for every OPD slot: empty, non-empty, unreadable, invalid code pointer, exact entry, containing function, and unresolved executable pointer.
- The runner reconciles the resolution summary against the OPD seeder's slot and pointer counts, then against the per-function CSV counts. A mismatch aborts the run before any output is promoted.
- The comparison report exposes exact-entry versus containing-only evidence and adds separate candidate counters. Phase 50 extends each function's TOC evidence with the mapped memory block name, permissions and relative offset.
- Legacy Phase 48 OPD CSV files remain readable; they are marked NOT_RECORDED for the new resolution classification rather than being silently upgraded.

## Validation boundary

Synthetic CSV fixtures and mocked Ghidra commands validate accounting and rollback behavior. EXACT_ENTRY_MATCH means the descriptor's code pointer equals a function entry in Ghidra. CONTAINING_FUNCTION_ONLY is weaker navigation evidence. Neither proves that the TOC is correct or that a function can be safely called from Campaign. No retail PPU ELF was analyzed in CI.
