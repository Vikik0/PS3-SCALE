# DSF MOD MENU build and hardware gates

## Implemented source

The separate installer source is in DSF-MOD-MENU-Installer. It bundles a pinned upstream Simple Cheat Menu SPRX and uses a short PS-button trigger. It does not add a gameplay controller hook and does not modify the game EBOOT or PARAM.SFO.

## CI build gates

- [ ] Download the pinned SPRX from the upstream release.
- [ ] Verify the exact SHA-256 before packaging.
- [ ] Run host tests for idempotent boot_plugins line insertion/removal.
- [ ] Generate and validate the XMB icon.
- [ ] Build ELF, SELF and GNPDRM PKG using PSL1GHT/Tiny3D.
- [ ] Validate PKG magic, revision, size and SHA-256.
- [ ] Upload the PKG as a GitHub Actions artifact.

## Console gates

Target: CECH-2504A, CEX 4.93, Cobra 8.5, Driver: San Francisco BLES00891 digital PKG.

- [ ] Confirm the exact installed PARAM.SFO APP_VER and EBOOT hash.
- [ ] Back up the existing boot_plugins.txt and plugin folder.
- [ ] Install the PKG and run the installer app.
- [ ] Confirm it appends only /dev_hdd0/plugins/simple-cheat-menu.sprx.
- [ ] Reboot and confirm the plugin loads without disabling other required plugins.
- [ ] Launch Driver: San Francisco and test a short PS press opens the overlay.
- [ ] Confirm a long PS press still opens the normal PS menu.
- [ ] Confirm D-pad/CROSS/CIRCLE navigation and exit behavior.
- [ ] Test coexistence with the exact installed webMAN MOD build.
- [ ] Remove through the installer and confirm the original plugin is restored.
- [ ] Confirm the game EBOOT.BIN and PARAM.SFO hashes are unchanged.

## Cheat safety

The public BLES00891 database does not establish green confidence for the user's displayed 1.00 build. Do not enable amber or red cheats if avoiding freezes is the priority. Unknown code entries are not relabelled or enabled by the installer.

## Release labels

- Source added: not a package.
- CI build passed: installable test-build PKG, not hardware-tested.
- Hardware-tested: only after the exact console passes all console gates above.

No 100% crash-free claim is technically supportable.
