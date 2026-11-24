# Phase 58 — Protect the complete native-analysis output bundle

## Problem

The one-command orchestrator initially checked only native_input_preflight.json and native_analysis_status.json before starting. If one of the six final Ghidra inventory artifacts already existed, the pipeline could write new preflight files and only then fail inside the inventory generator. Existing artifacts were not overwritten, but the output folder was left with partial status files and a misleading GHIDRA_ANALYSIS_FAILED result.

## Changes

- Before verification or writing, check both status files and all six OUTPUT_NAMES artifacts.
- Without --overwrite, fail immediately if any protected output exists.
- Preserve the existing explicit --overwrite behavior.
- Add a test proving that an existing inventory file remains unchanged, verification is not invoked, and no new preflight/status files are written.

## Validation boundary

This is local output-safety behavior. It does not change the analysis inputs, ELF interpretation, Ghidra output format or game binaries.
