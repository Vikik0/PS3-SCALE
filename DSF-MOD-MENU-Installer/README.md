# DSF MOD MENU Installer

This is a small PSL1GHT homebrew PKG that installs the open-source Simple Cheat Menu VSH plugin. The plugin is a generic in-game cheat overlay; it selects the cheat database by the currently running game's Title ID.

## Approved menu trigger

The user approved the plugin's existing **short PS-button press** trigger. A long PS press is left to the normal PS3 system menu. The installer does not attempt to poll the controller during raw gameplay and does not add a custom input hook.

In-game controls:
- Short PS press: open the overlay.
- D-pad up/down: select a cheat.
- CROSS: toggle the selected cheat.
- TRIANGLE: re-fetch the current game's cheat list (online mode).
- CIRCLE: return to the in-game XMB.
- PS: resume the game.

## What the PKG does

The PKG installs the pinned upstream plugin at:
- /dev_hdd0/plugins/simple-cheat-menu.sprx

It adds exactly one line to:
- /dev_hdd0/boot_plugins.txt

It preserves other plugin lines and keeps one-time backups:
- /dev_hdd0/plugins/simple-cheat-menu.sprx.dsf-backup
- /dev_hdd0/boot_plugins.txt.dsf-backup

It creates /dev_hdd0/tmp/simple-cheat-menu/settings.txt with mode=fetch only if that settings file does not already exist. This avoids the upstream default contribute mode and does not overwrite an existing user's settings. Fetch downloads cheats but does not send votes.

The app never writes to the game's EBOOT.BIN, PARAM.SFO, or other game files. Remove Plugin deletes only its own boot entry and restores the plugin backup if one exists. It leaves downloaded cheat data and user settings untouched.

## Pinned upstream component

- Project: https://github.com/mohasi/ps3-dev/tree/main/plugins/simple-cheat-menu
- Release: https://github.com/mohasi/ps3-dev/releases/tag/191-3541f8d
- Asset: simple-cheat-menu.sprx
- SHA-256: cbb556217faf63d2ae2c5480318397f0f4b1fa4b1ad52b39f55384c59a10ee90
- License: Apache-2.0; attribution and license copy are retained under third_party/.

## Target console and game

Target reported for this build:
- PS3 Slim CECH-2504A
- CEX CFW 4.93
- Cobra 8.5
- Driver: San Francisco, BLES00891, digital PKG
- XMB displayed game version 1.00; installed PARAM.SFO APP_VER and EBOOT hash have not yet been read from the console.

The plugin is intended for CFW with Cobra support. Its upstream author documents CFW operation and the PS-button trigger. A PSX-Place discussion includes the author reporting use alongside webMAN MOD 1.47.48, but this is not a guarantee for every plugin combination or every webMAN build.

## Driver: San Francisco cheat data

The public BLES00891 cheat file currently contains named entries for:
- Car Health Never Decreases
- Infinite WP
- Max Money On Gain

It also contains three entries labelled Memory Unknown Code 1–3; these are deliberately not curated or renamed by this installer.

The public cheat file does not currently provide a confirmed green confidence score for the user's displayed 1.00 build. The overlay uses green for confirmed on the exact build, amber for unproven, and red for codes only known to work on another build. Do not enable amber or red entries on the 1.00 game build if avoiding crashes is the priority. The installer does not falsely mark these cheats as tested.

## Features not bundled

The separate RageX V1.0.3T announcement describes traffic/pedestrian hiding, vehicle spawning and a Square-button speed boost. It is a third-party binary installer with no source code available in the release post, and the author says minor issues remain. Those functions are not included in this open-source installer and must not be presented as if this plugin provides them.

## Build

The GitHub Actions workflow downloads the pinned SPRX, verifies its SHA-256, builds the installer with the repository's PSL1GHT/Tiny3D toolchain, validates the PKG header, and uploads dsfmod.gnpdrm.pkg.

A successful GitHub build proves the package compiled and passed host-side checks. It does not prove it has been installed and tested on the user's physical PS3. Hardware testing is still required for the plugin-load, menu-open, webMAN coexistence, game launch, and uninstall paths.

## Safety and rollback

No PS3 plugin can be honestly guaranteed 100% crash-free on every console/game combination. Keep a recovery route available before enabling a boot plugin. If the console fails to boot normally, use the existing recovery method for the user's CFW to remove the added line from /dev_hdd0/boot_plugins.txt or restore the .dsf-backup files. Do not delete the other plugin entries.
