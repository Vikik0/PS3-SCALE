# Implementation base discovered: simple-cheat-menu

## Why this is the best existing PS3-side foundation found

Repository: https://github.com/mohasi/ps3-dev

Source path:
- https://github.com/mohasi/ps3-dev/tree/main/plugins/simple-cheat-menu

The project's own README describes a real in-game PAF overlay, live cheat toggles, read-back verification, original-byte snapshots and restore, AoB scanning, automatic cheat downloads, community confidence scores, and optional live texture patches. It operates as one VSH plugin and writes to game memory cross-process through Cobra/PS3MAPI; it does not inject a module into the game process.

The repository is Apache-2.0 licensed (root LICENSE and NOTICE must be retained if this code is reused or modified). The cheat data is separate and attributes original Artemis authors.

## BLES00891 is already represented in its cheat database

Companion database:
- https://github.com/mohasi/game-cheats/blob/main/source/BLES00891.txt

The file contains:
- Car Health Never Decreases
- Infinite WP (999,999)
- Max Money On Gain
- Three entries labelled Memory Unknown Code 1–3

Do not enable the unknown entries. The public Artemis codelist indexes BLES00891 as version 01.01 (2.1 KB). The user's XMB screen showed 1.00, so the exact installed PARAM.SFO APP_VER must be read before applying any of these codes. The plugin itself reads APP_VER from the installed PKG directory first, which is a useful design feature for this digital installation.

## Existing prebuilt artifact found

GitHub release tag:
- https://github.com/mohasi/ps3-dev/releases/tag/191-3541f8d

The release API lists `simple-cheat-menu.sprx` (59,315 bytes; SHA-256 `cbb556217faf63d2ae2c5480318397f0f4b1fa4b1ad52b39f55384c59a10ee90`). This is a third-party build, not a DSF-specific build and not a PKG installer. Its exact runtime compatibility with the user's CEX 4.93 / Cobra 8.5 console still requires hardware testing.

## Critical hotkey limitation

The plugin's documented and implemented trigger is a short PS-button press. It uses the PS overlay as the safe point where VSH regains controller ownership. Its source explicitly says:
- During normal gameplay, the game owns the pad and VSH `cellPadGetData` is silent.
- Continuously polling the raw HID handle across game-launch/controller-ownership transitions can hard-lock LV2.
- Raw HID reads are therefore restricted to the period when the PS overlay is open.

This means changing the VSH-only trigger to D-pad UP + SELECT is not a safe one-line keybind edit. To detect that combo while the game owns the controller, a game-process input hook or a separate sensor module would be needed. That is a materially different, game-sensitive injection path and cannot honestly be called crash-free without BLES00891-specific implementation and console tests.

## Recommended release decision

Two possible targets must not be conflated:

1. **Safer generic VSH menu** — reuse/adapt the Apache-2.0 simple-cheat-menu foundation, keep its PS-button trigger, install via a reversible PKG installer, and test on CEX 4.93 / Cobra 8.5.
2. **Exact UP + SELECT trigger** — requires a separate game-process input hook for BLES00891 and additional rendering/input testing. Do not claim compatibility or package it as a stable release until that hook has been proven on the user's exact EBOOT.

The user asked for UP + SELECT. Do not silently substitute the PS button without consent.
