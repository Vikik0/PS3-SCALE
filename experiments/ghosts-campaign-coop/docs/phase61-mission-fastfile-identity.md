# Phase 61 — Pin and verify the three supplied mission FastFiles

## Purpose

The mission ownership audit uses the Phase 31 extraction bundle. Phase 61 adds a reusable verifier for the exact three user-supplied FastFiles so future extraction/audit runs can reject a different revision before analysis.

## Pinned identities

- black_ice.ff — 91,355,961 bytes; SHA-256 950726399e16f55fd565438936ec59231649940c89763a87c805409536837532
- carrier.ff — 87,088,544 bytes; SHA-256 e3a616545f18a7dcae006b4e32264c56b01d627650ea574f3fe710e02543caee
- cornered.ff — 93,933,243 bytes; SHA-256 1bcb1da55d7e5a893de2a513e6df9b5cd3786e4d642dc76e090ea551cdf39395

All three supplied attachments were checked directly: their sizes, SHA-256 values, eight-byte IWff0100 magic and version 0x0000022E match the pinned Phase 31 extraction inputs.

## Changes

- Add a read-only streaming SHA-256 verifier for the three mission FastFiles.
- Check exact size, SHA-256, eight-byte FastFile magic and the big-endian version field at bytes 8–11; report missing/unreadable/mismatched inputs without copying binary contents.
- Add tests for a valid set, a hash mismatch, a missing role and a missing file.

## Validation boundary

A matching hash proves that the audited FastFile bytes match the pinned inputs. It does not prove that a FastFile is a retail update, nor does it establish native Campaign Player 2 support.
