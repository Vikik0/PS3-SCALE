# PC-first Ghosts test loop

This workflow reduces manual PS3 testing. Game files remain local and are never uploaded to GitHub.

## 1. Run host tests on Windows

Requirements: Python 3.10+, PowerShell 5.1 or 7, and a host C compiler on PATH (GCC such as MSYS2 MinGW-w64, or LLVM Clang).

From the repository root, run:

~~~powershell
powershell -ExecutionPolicy Bypass -File .\experiments\ghosts-campaign-coop\tools\run_host_tests.ps1
~~~

This compiles and runs the portable C tests and Python suite, including the 10,025-check Ghosts host runtime contract simulator. It does not build a PS3 PRX, boot RPCS3, write to a console, or create a PKG.

## 2. Identify the local game and RPCS3 installation

For a disc dump folder containing PS3_GAME:

~~~powershell
python .\experiments\ghosts-campaign-coop\tools\rpcs3_preflight.py --game-root "D:\Games\Ghosts" --rpcs3-root "D:\Emulators\RPCS3" --expected-title-id BLUS31270 --output-json "$env:TEMP\ghosts-rpcs3-preflight.json"
~~~

For an installed RPCS3 game folder, pass dev_hdd0\game\BLUS31270 as game-root. You can also pass the PS3_GAME directory itself.

The preflight is read-only. It checks PARAM.SFO and Title ID, fingerprints EBOOT.BIN, identifies whether it is an SCE container or plain ELF, finds the RPCS3 executable and log, extracts PPU hashes, and reads RPCS3 Pad 0 / Pad 1 handler-binding entries when present. Two non-null bindings are emulator-side mapping evidence only; they do not prove controller input or Campaign split-screen.

It does not decrypt SELF, infer TU 1.16 from APP_VER, configure controllers, launch the game, or generate an executable patch. A success result means only READY_FOR_BASELINE_BOOT.

## 3. First RPCS3 baseline capture

1. Open RPCS3 normally with the GUI. Do not use no-GUI mode during visual/debug work.
2. Confirm the selected game matches the Title ID from preflight.
3. Configure two independent controller ports and test them in an existing offline local multiplayer mode first. After boot, check the preflight report for Pad 0 and Pad 1 bindings, then verify that each controller actually moves the correct local player.
4. Boot Ghosts once. Do not enable any unverified patch or PRX.
5. Close the game cleanly and rerun preflight to capture the PPU hash from RPCS3.log.
6. Inspect the log around that hash and confirm it belongs to the Ghosts boot.

The PPU hash is a build identity, not proof of TU 1.16. The installed update still needs independent verification.

## 4. Loader gate

Do not generate a Ghosts-specific loader until we have the exact Title ID and TU 1.16 identity, PPU hash, matching decrypted ELF, ELF load map, OPD/TOC, required PRX imports, a safe injection site, and a harmless PRX load/unload test. A loader for another game, region, or update is not reusable.

## 5. Iteration after the loader is proven

- Portable code edit: local host tests + GitHub Actions.
- PS3 adapter edit: cross-build PRX in GitHub Actions.
- After RPCS3 loading is verified: replace only PRX/patch in the persistent emulator install and restart the game.
- Physical PS3: milestone gates for CEX behavior, real controllers, rendering, thermals and stability.

No installable PKG should be produced for every code edit.
