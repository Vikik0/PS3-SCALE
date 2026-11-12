# DSF MOD MENU — BLES00891

Target: Driver: San Francisco (PS3), European Title ID BLES00891, digital PKG.

Target console reported by the user:
- PS3 Slim CECH-2504A
- CEX CFW 4.93
- Cobra 8.5
- XMB game information displays version 1.00

## Approved menu trigger

The user approved the upstream Simple Cheat Menu trigger: **short PS-button press**. A long PS press is left to the normal PS3 system menu. The unsafe D-Pad UP + SELECT polling hook is not being added.

## Current implementation

The branch now contains a separate PSL1GHT installer app under DSF-MOD-MENU-Installer. Its intended PKG:
- Bundles a pinned, hash-verified upstream Simple Cheat Menu SPRX.
- Installs it to /dev_hdd0/plugins/simple-cheat-menu.sprx.
- Adds one boot_plugins.txt entry without duplicating or removing other plugins.
- Creates mode=fetch only if the user has no existing settings file.
- Backs up an existing plugin and boot_plugins file once.
- Includes a remove option that deletes only its own boot entry and restores the plugin backup.
- Never modifies the game's EBOOT.BIN or PARAM.SFO.

The installer source and CI workflow are being built. Until the GitHub Actions build passes, there is no completed PKG artifact.

## Community research

The Simple Cheat Menu plugin provides an in-game PAF overlay, live toggles, read-back verification, AoB cheats, per-build confidence badges, background cheat downloads and texture-patch support.

For BLES00891, the public database lists Car Health Never Decreases, Infinite WP, Max Money On Gain, and three unknown memory entries. The user's XMB displayed 1.00, while the public Artemis index is associated with 01.01. No exact-build green confidence is established. Do not activate amber or red entries if stability is the priority.

TEAM RageX V1.0.3T separately advertises hide traffic/pedestrians, vehicle spawning and a Square-button speed boost. That is a third-party binary release with no source in its announcement and acknowledged remaining minor issues. Those features are not falsely claimed as part of this open-source plugin build.

## Validation status

- [x] Title ID and reported console target recorded.
- [x] User approved short PS-button trigger.
- [x] Upstream SPRX pinned by SHA-256.
- [x] Reversible installer source added.
- [ ] GitHub Actions host tests and PKG build.
- [ ] Install test on CECH-2504A / CEX 4.93 / Cobra 8.5.
- [ ] Cold-boot and plugin-load test.
- [ ] Game launch and overlay open/close test.
- [ ] webMAN coexistence test with the user's installed version.
- [ ] Remove and rollback test.

A successful CI build is not a hardware test. No 100% crash-free claim is made.
