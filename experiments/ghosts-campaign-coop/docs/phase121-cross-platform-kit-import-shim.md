# Phase 121 — cross-platform capture-kit import shim

## Problem found

The kit presents its tools under a user-facing Tools directory, while the Python analyzer imports modules from a lowercase tools package. Windows normally resolves that case difference, but Linux and other case-sensitive filesystems do not. A clean extracted kit therefore passed its file-integrity check but failed when the combined analyzer was launched.

## Changes

- Add a small lowercase tools package shim in the generated kit. Its package search path points to the existing Tools directory; no second copy of the analyzer modules is created.
- Require the shim in the capture-kit dependency verifier.
- Change the builder tests to use the real analyzer modules instead of placeholder files.
- Extract a built ZIP in the test and run the combined analyzer's --help command from inside the extracted kit. This exercises the actual import chain on a case-sensitive host.

## Validate

~~~bash
python -m unittest discover -s tests -p 'test_build_capture_kit.py' -v
python -m unittest discover -s tests -p 'test_verify_capture_kit.py' -v
~~~

The import smoke test must complete without requiring a PS3, memory dumps, Ghidra installation, or network access.

## Runtime boundary

This fixes host-side packaging portability only. It does not prove a second native Campaign player, create a Local Client, migrate mission handlers, or modify any retail EBOOT, SELF, or FastFile. Player 2 remains NOT_PROVEN and shared interactions remain 0/4 ready.
