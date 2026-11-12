# DSF MOD MENU — CI build report

Date: 2026-11-13
Branch: feature/dsf-mod-menu-bles00891
Commit: ba9df96e00ef6205bebc26b1b9ad87db228216b1

## GitHub Actions
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/43308505549
- Conclusion: success
- Artifact: DSF-MOD-MENU-Installer-test-build
- Artifact ID: 13983524872
- Artifact ZIP SHA-256: d005e8a745aaa51ce170092eab0a994e8a132a16688b37a665f70e15c545ff64

## PKG
- File: dsfmod.gnpdrm.pkg
- Size: 159,248 bytes
- PKG magic: 7f504b47
- PKG revision: 8000
- SHA-256: 90ba2498c2ee47f703df81912c514eac9137cf7daaa6192c3e9fe993a3413c2e

## Included plugin
- File: simple-cheat-menu.sprx
- Size: 59,315 bytes
- SHA-256: cbb556217faf63d2ae2c5480318397f0f4b1fa4b1ad52b39f55384c59a10ee90

## Checks passed
- Plugin asset downloaded from the pinned upstream release and its SHA-256 matched.
- Host installer policy tests passed.
- XMB icon PNG validation passed at 320x176.
- PPU ELF link passed.
- CEX SELF generation passed.
- GNPDRM PKG generation passed.
- PKG magic and revision checks passed.
- GitHub Actions artifact upload passed.

## Not yet tested
- Installing on the user's CECH-2504A.
- Boot plugin loading on CEX 4.93 / Cobra 8.5.
- Short PS-button overlay behavior in BLES00891.
- Coexistence with the exact installed webMAN MOD version.
- Removal and backup restoration on hardware.

This report certifies a successful CI package build only. It does not certify crash-free operation on a physical console.
