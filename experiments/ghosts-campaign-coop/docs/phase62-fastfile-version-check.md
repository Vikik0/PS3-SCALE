# Phase 62 — Validate the FastFile version field explicitly

## Problem

Phase 61 pinned the expected FastFile version in its metadata but checked only size, SHA-256 and magic. The hash already identifies the bytes, but explicitly parsing the version field makes the report easier to diagnose and prevents an incorrect version expectation from silently remaining unused.

## Changes

- Read the first 12 bytes of each FastFile.
- Decode the eight-byte IWff0100 magic.
- Parse bytes 8–11 as a big-endian 32-bit version and compare it with 0x0000022E.
- Report the actual version and a separate version check.
- Add a test where the file size, magic and SHA-256 are all pinned to the supplied bytes but the version field is wrong.

## Validation boundary

The version field is a structural identity check. The SHA-256 remains the authoritative byte identity; no game content is modified or interpreted by this verifier.
