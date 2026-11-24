# Phase 63 — Verify FastFiles from one stable file snapshot

## Problem

The previous verifier called stat, opened the file to read its header, then opened it again to calculate SHA-256. If a FastFile changed between those operations, the report could combine a size/header from one version with a digest from another.

## Changes

- Read the header and calculate SHA-256 in one streaming pass through one open file descriptor.
- Capture descriptor metadata before and after reading: device, inode, size, modification time and change time.
- Confirm the path still resolves to the same file after reading.
- Reject a truncated/growing file, an in-place modification, or path replacement during verification with an explicit retry error.
- Keep the original read-only behavior: no binary contents are copied into reports or modified.

## Validation

Synthetic tests force both an in-place metadata change and a path replacement between the before/after checks. Existing identity tests continue to verify exact SHA-256, size, magic and big-endian version matching.

The verifier identifies a stable byte snapshot; it does not prove native Campaign co-op support.
