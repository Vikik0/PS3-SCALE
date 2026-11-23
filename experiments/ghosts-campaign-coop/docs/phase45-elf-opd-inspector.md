# Phase 45 — ELF inspector OPD range and descriptor diagnostics

## Purpose

The host ELF inspector previously exposed the section containing the ELF entry and listed only descriptors whose code pointer landed in an executable PT_LOAD range. Empty slots, invalid code pointers, unmapped TOCs, section alignment and trailing bytes were not visible. That made the inspector less useful for selecting the exact OPD range to pass to the Ghidra seeder.

## Changes

- The inspector now reports total/scanned OPD slots, empty/non-empty descriptors, valid/invalid code pointers and trailing bytes.
- It reports distinct, mapped and unmapped TOC values, plus zero-TOC descriptor counts.
- Each parsed entry descriptor reports code-pointer alignment/executable status and TOC mapping status.
- The common non-zero TOC is reported only from valid code-pointer descriptors and includes an explicit mapped flag.
- The inspector emits operator_seed_range_candidate only when the containing section is readable, 8-byte aligned by address and declared section alignment, has a size divisible by 8, fits the unsigned 32-bit PPU address space, and contains at least one valid executable code pointer.
- The inspector version advances to 0.1.2. It remains read-only and does not decrypt SELF files or modify game bytes.

## Validation boundary

Synthetic ELF fixtures cover valid, empty and invalid descriptors, unmapped TOC values, under-aligned sections and trailing bytes. This is parser validation only. The exact user EBOOT ELF is still required to produce real OPD evidence; no function hook or Player 2 path is inferred.
