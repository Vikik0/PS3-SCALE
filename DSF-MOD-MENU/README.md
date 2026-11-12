# DSF MOD MENU — BLES00891 research baseline

Project target: **Driver: San Francisco (PS3), BLES00891, Digital PKG**.

Target console recorded from the user's setup:
- PS3 Slim CECH-2504A
- CEX CFW 4.93
- Cobra 8.5
- Game ID shown by webMAN: `BLES00891`
- XMB game information screen displayed version: `1.00`

## Status

This branch is a research and compatibility baseline. **It does not contain a working in-game SPRX, memory patches, or an installable DSF MOD MENU PKG yet.** A package must not be called ready until the overlay and each feature have been built and tested on the target console.

## Intended controls

- Open/close: **D-Pad UP + SELECT**
- Navigation: D-Pad UP/DOWN
- Confirm: CROSS
- Back: CIRCLE
- Toggle selected option: CROSS

The requested hotkey is not implemented in a runtime plugin yet.

## Community feature findings

### PS3-specific mod menu reference

TEAM RageX announced a PS3 menu for Driver: San Francisco with:
- Hide random traffic vehicles and pedestrians
- Spawn vehicles
- Vehicle speed boost on SQUARE
- Claimed Disc/Digital and all-region support

The announcement says a few minor issues remain. Its PKG is a third-party binary; no source code was found in the announcement. It is a reference, not code to copy or a basis for a crash-free guarantee.

### Existing PS3 cheat data for BLES00891

The public Artemis codelist indexes **BLES00891 / Driver San Francisco / version 01.01** with a 2.1 KB list. A separate community game-cheats text file includes:
- Car Health Never Decreases
- Infinite WP
- Max Money On Gain
- Three entries labelled Memory Unknown Code 1–3

The unknown entries are excluded from any future menu until their effect and exact version are independently established. The listed code data must not be applied to the user's displayed 1.00 build without verifying the installed PARAM.SFO APP_VER and executable identity.

### PC-only mods — not PS3 ports

ModDB lists PC mods such as Felony Rework, Busted, Restored Split Screen, Moods Rework, Demolition Derby, Device Refresher, and a PC Mod Menu. These are not automatically compatible with PS3. They are research inspiration only; no PC scripts or binaries should be copied into a PS3 PKG.

### FPS / graphics patches

A public PS3 FPS patch collection lists Driver: San Francisco for **BLUS30536 v1.00**, not BLES00891. That is a region/title-ID mismatch and must not be applied to this target. No verified BLES00891 FPS-unlock or resolution patch was found in that collection.

## Runtime architecture required

A normal XMB homebrew PKG cannot remain alive as an overlay after the user starts a separate game. The intended design therefore needs:
1. An installer that places a dedicated plugin and profile under a project-owned path.
2. A reversible plugin-load configuration for Cobra CFW.
3. A VSH-side overlay for drawing the menu above the running game.
4. A game-process module or a guarded PS3MAPI backend for game-specific features.
5. A title/version gate that refuses all memory writes when Title ID, APP_VER, EBOOT identity, or expected original bytes do not match.
6. An uninstaller that removes only project-owned files and restores only the exact boot-plugin line it added.

Do not replace the game's original EBOOT or PARAM.SFO as an installation shortcut.

## Build and test gates

- [ ] Read the installed `/dev_hdd0/game/BLES00891/PARAM.SFO` and record TITLE_ID + APP_VER.
- [ ] Hash the installed EBOOT.BIN and identify its update level.
- [ ] Build the VSH plugin and game-process component with a toolchain that supports the required VSH/PAF interfaces.
- [ ] Confirm the plugin loads on CEX 4.93 / Cobra 8.5 without the game running.
- [ ] Confirm D-Pad UP + SELECT toggles the overlay without triggering game actions.
- [ ] Confirm the overlay preserves/restores render state and does not alter the game's frame pipeline when closed.
- [ ] Confirm the plugin does not attach to unrelated games.
- [ ] Test each feature separately in Story Mode, with a clean save and a rollback path.
- [ ] Test installer, reboot, launch, uninstall, and clean reboot on the actual CECH-2504A.

No feature is marked working until its test evidence is recorded.
