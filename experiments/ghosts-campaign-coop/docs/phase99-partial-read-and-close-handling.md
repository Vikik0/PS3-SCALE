# Phase 99 — partial-read accumulation and close-error handling

## Change

The PS3 loader no longer assumes one `cellFsRead` call returns the entire file. It accumulates successful short reads until EOF or until the 65,536-byte staging buffer is full, then performs the one-byte overflow probe.

Additional fail-closed behavior:
- A read error or impossible over-reported byte count closes the descriptor and rejects the snapshot.
- A file larger than the staging buffer is rejected and the buffer is cleared.
- A close failure now returns the explicit `GHC_SCRIPT_OVERRIDE_CLOSE_FAILED` status.
- No parsed metadata or memory address is exposed after a failed load.

## Verification

- Python suite: **536/536 passed**.
- Portable wrapper parser: **10/10 passed**.
- Mocked file-loader tests: **12/12 passed**, including fragmented reads, partial-read failure and close failure.
- PS3 PRX bootstrap, both read-only controller probes and importer link validation passed.
- CI run [#1578](https://github.com/Vikik0/PS3-SCALE/actions/runs/45721292708): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **537,208 bytes**, SHA-256 `280cfc42a141af788b8e129dc782a01e021f0b0520350228ed43336f535a48a4`; artifact ID `15072062945`.

## Boundary

The file-I/O tests use a host mock. They do not prove a live PS3 filesystem read or a retail Campaign script hook.
