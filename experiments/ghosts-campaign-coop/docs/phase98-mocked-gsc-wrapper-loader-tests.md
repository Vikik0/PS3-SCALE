# Phase 98 — mocked PS3 GSC file-loader tests

## Change

Added a host-only mock of the Cell FS API and exercised the actual `script_override.c` loader together with the portable parser.

The tests cover successful load and diagnostic getters, open failure, first-read failure, extra-byte probe failure, oversized files, exact-capacity files, bad magic, invalid lengths, and clearing a previously ready snapshot after a failed reload.

## Verification

- Python suite: **536/536 passed**.
- Portable wrapper parser: **10/10 passed**.
- Mocked file-loader tests: **9/9 passed**.
- PS3 PRX bootstrap and read-only probes passed.
- CI run [#1577](https://github.com/Vikik0/PS3-SCALE/actions/runs/45720980397): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **536,971 bytes**, SHA-256 `064ae4675fa1ca22264a4efaba3e10f75d27bf10c281cc6b6051c0f89851d24b`; artifact ID `15071488495`.

## Boundary

The Cell FS implementation is mocked on the host. No live PS3 file-system read was performed.
