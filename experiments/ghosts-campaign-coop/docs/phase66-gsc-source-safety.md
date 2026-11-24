# Phase 66 — Preserve the read-only GSC source boundary

## Problem

The combined runner writes a JSON report after analysis. If the caller accidentally points --output-json at one of the source .gsc files and enables --overwrite, the report writer could replace an input script after the audits complete. That would violate the tool's read-only contract.

## Changes

- Resolve the requested output path and every hashed source GSC path.
- Reject output paths that resolve to any input source file, including a symlink alias.
- Keep --overwrite available for existing report files, but never allow it to override this source-file protection.
- Validate mission mapping keys as non-empty strings.

## Validation

A regression test attempts to use a source GSC file as the output with overwrite enabled and confirms the tool rejects the operation and preserves the original bytes.

## Boundary

This is output-safety hardening for the local static analysis runner. Source GSC and retail FastFiles remain read-only inputs.
