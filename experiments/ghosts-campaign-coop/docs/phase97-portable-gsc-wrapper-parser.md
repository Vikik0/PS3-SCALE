# Phase 97 — portable GSC wrapper parser

## Change

Extracted GSC wrapper validation from the PS3 file-I/O adapter into a portable C module used by both the PRX and host tests.

The parser validates:
- `GSC\0` magic.
- Non-zero compressed, stack and bytecode lengths.
- Compressed and bytecode segments against the supplied capacity without unsafe addition.
- Exact equality between declared total length and bytes read.
- FNV-1a hash over the complete wrapper.

It clears the output metadata on every failed parse. The PS3 loader now delegates to this same parser.

## Verification

- Python suite: **536/536 passed**.
- Portable parser C tests: **10/10 passed**.
- PRX bootstrap and all read-only probes built/validated.
- CI run [#1575](https://github.com/Vikik0/PS3-SCALE/actions/runs/45720639607): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **536,971 bytes**, SHA-256 `064ae4675fa1ca22264a4efaba3e10f75d27bf10c281cc6b6051c0f89851d24b`; artifact ID `15072465712`.

## Compiler sample

The CI-produced IW6/PS3 session-bridge GSCBIN was inspected read-only: wrapper header values were compressed length **740**, stack length **6,795**, bytecode length **3,072**, total file size **3,828 bytes**. The parser's segment arithmetic matches the wrapper's 16-byte header plus compressed and bytecode segments.

## Boundary

The parser validates file structure only. It does not create an engine ScriptFile object or execute the staged GSC.
