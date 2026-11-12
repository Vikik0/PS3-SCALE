# DSF MOD MENU — BLES00891

Target: Driver: San Francisco (PS3), European Title ID BLES00891, digital PKG.

Target console reported by the user:
- PS3 Slim CECH-2504A
- CEX CFW 4.93
- Cobra 8.5
- XMB game information displays version 1.00

## Approved menu trigger

The user approved the upstream **short PS-button press** trigger. A long PS press is left to the normal PS3 system menu. The unsafe D-Pad UP + SELECT polling hook is not being added.

## Build status

A real GNPDRM PKG was built successfully by GitHub Actions:
- Workflow run: https://github.com/Vikik0/PS3-SCALE/actions/runs/43308505549
- Artifact: DSF-MOD-MENU-Installer-test-build
- Artifact ID: 13983524872
- PKG: dsfmod.gnpdrm.pkg
- Size: 159,248 bytes
- SHA-256: 90ba2498c2ee47f703df81912c514eac9137cf7daaa6192c3e9fe993a3413c2e

CI checks passed:
- Pinned SPRX download and SHA-256 verification.
- Installer host policy tests.
- 320x176 XMB icon validation.
- PPU ELF and CEX SELF build.
- GNPDRM PKG generation.
- PKG magic and revision checks.

This is a **CI-built test package**, not yet a hardware-tested release.

## What the PKG installs

The PKG installs the open-source Simple Cheat Menu VSH plugin. It is a generic in-game overlay that selects cheat data by the running game's Title ID.

- Plugin path: /dev_hdd0/plugins/simple-cheat-menu.sprx
- Boot entry: /dev_hdd0/boot_plugins.txt
- Settings: /dev_hdd0/tmp/simple-cheat-menu/settings.txt

The installer app adds only its own boot entry, preserves other plugin lines, backs up an existing plugin and boot_plugins file once, and includes a Remove option. It never edits the game's EBOOT.BIN or PARAM.SFO. A new settings file defaults to mode=fetch, so cheat data downloads but anonymous votes are not sent. Existing settings are preserved.

## In-game controls

- Short PS press: open the overlay.
- D-pad up/down: select a cheat.
- CROSS: toggle the selected cheat.
- TRIANGLE: re-fetch the current game's cheats.
- CIRCLE: return to the in-game XMB.
- PS: resume the game.

## Driver: San Francisco cheat data

The public BLES00891 file contains named entries for Car Health Never Decreases, Infinite WP, and Max Money On Gain, plus three unknown memory entries. The public data does not currently provide a confirmed green confidence score for the user's displayed 1.00 build. Do not activate amber or red entries if stability is the priority.

TEAM RageX V1.0.3T separately advertises hiding traffic/pedestrians, vehicle spawning and a Square-button speed boost. That is a third-party binary release with no source in its announcement and acknowledged remaining minor issues. Those features are not included in this open-source installer.

## Remaining hardware checks

- [ ] Install the PKG on CECH-2504A / CEX 4.93 / Cobra 8.5.
- [ ] Run the installer app and choose INSTALL / UPDATE PLUGIN.
- [ ] Reboot and confirm plugin load.
- [ ] Launch BLES00891 and test a short PS press.
- [ ] Confirm long PS press still opens the normal PS menu.
- [ ] Test webMAN coexistence with the exact installed build.
- [ ] Test Remove and backup restoration.
- [ ] Confirm game EBOOT.BIN and PARAM.SFO hashes remain unchanged.

A successful CI build does not prove hardware compatibility. No 100% crash-free claim is made.
