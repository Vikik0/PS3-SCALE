# Implementation base: Simple Cheat Menu

The user approved the upstream **short PS-button press** trigger. This avoids adding a raw-controller polling hook during gameplay.

## Upstream plugin

- Repository: https://github.com/mohasi/ps3-dev/tree/main/plugins/simple-cheat-menu
- Release: https://github.com/mohasi/ps3-dev/releases/tag/191-3541f8d
- Asset: simple-cheat-menu.sprx
- Size: 59,315 bytes
- SHA-256: cbb556217faf63d2ae2c5480318397f0f4b1fa4b1ad52b39f55384c59a10ee90
- License: Apache-2.0

The plugin draws a PAF overlay, toggles cheats with live read-back verification, supports AoB searches, shows per-build confidence badges, and can download cheat files per Title ID. It writes to the game process through Cobra/PS3MAPI and does not replace the game's EBOOT.

## Installer design

The separate PSL1GHT app in DSF-MOD-MENU-Installer bundles the pinned SPRX and installs it to /dev_hdd0/plugins/simple-cheat-menu.sprx. It adds exactly one line to /dev_hdd0/boot_plugins.txt, preserves other entries, creates one-time backups, and includes a remove/restore option.

The installer creates settings mode=fetch only when no settings file already exists. This downloads cheat data but does not submit anonymous votes by default. Existing user settings are preserved.

## BLES00891 data and version caution

The public BLES00891 file includes:
- Car Health Never Decreases
- Infinite WP
- Max Money On Gain
- Three unknown memory entries

The user's XMB shows version 1.00. The public Artemis codelist indexes BLES00891 at 01.01, and the shared cheat file has no confirmed green score for the user's exact 1.00 build. Do not treat these cheats as tested for 1.00; avoid amber/red entries if stability is the priority.

## RageX reference

TEAM RageX V1.0.3T advertises hiding traffic/pedestrians, spawning vehicles and a Square-button boost. It is a separate third-party binary package with no source code in the announcement, and the release post acknowledges remaining minor issues. Those functions are not part of this open-source installer and are not claimed as implemented.

## Validation

CI can validate the installer source, build the PKG, and verify its package header. It cannot validate the plugin on the user's physical PS3. Hardware testing is required for boot loading, PS-button overlay behavior, webMAN coexistence, game launch, and uninstall/rollback.
