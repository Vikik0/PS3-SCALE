# Phase 36 — One-shot Campaign/Multiplayer PPU evidence bundle

## Purpose

Phases 34 and 35 added split-screen builtin XREF export and automated headless generation. Phase 36 completes the pipeline by running the Campaign/Multiplayer XREF comparison inside the same transaction, so one invocation creates a complete evidence bundle.

## Pipeline

For each exact TU 1.16 reconstructed ELF, the runner:

1. imports the build into a separate temporary Ghidra project;
2. exports the normalized PPU function inventory;
3. exports split-screen builtin string XREFs;
4. validates both CSV files and joins caller entries to that build's inventory.

After both builds validate, it compares caller fingerprints across Campaign and Multiplayer and writes the combined JSON report.

## Six-file output set

- campaign_ppu_inventory.csv
- multiplayer_ppu_inventory.csv
- campaign_builtin_xrefs.csv
- multiplayer_builtin_xrefs.csv
- splitscreen_builtin_xrefs.json
- ppu_inventory_manifest.json

The manifest records source ELF hashes, inventory hashes/counts, XREF hashes/counts, comparison JSON hash and Multiplayer-only lead count. All six outputs are staged and promoted together; a failure in either Ghidra pass, CSV validation, comparison, or promotion prevents a partial evidence set.

## Usage

The Phase 28 command is unchanged. Run tools/run_ghidra_ppu_inventory.py once with the exact Campaign and Multiplayer ELF pair. The output folder contains the comparison JSON automatically; no separate comparer command is needed for the normal path.

## Validation boundary

Host tests use synthetic ELF/CSV fixtures and a mocked Ghidra process. They verify both post-scripts, cross-build fingerprint matching, XREF validation, manifest hashes and atomic promotion. CI does not execute Ghidra itself. Real decrypted TU 1.16 PPU images and actual Ghidra results are still required before a native local-client initializer can be identified or a hook considered.
