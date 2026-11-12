# DSF MOD MENU Installer

This is a PSL1GHT homebrew PKG that installs the open-source Simple Cheat Menu VSH plugin.

## Build result

GitHub Actions completed successfully:
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/43308505549
- Artifact: DSF-MOD-MENU-Installer-test-build
- PKG: dsfmod.gnpdrm.pkg
- Size: 159,248 bytes
- SHA-256: 90ba2498c2ee47f703df81912c514eac9137cf7daaa6192c3e9fe993a3413c2e

The package passed CI host tests, PPU ELF/SELF compilation, PKG generation, and PKG header/revision checks. It has not yet been installed on the target physical console.

## Install steps

1. Install DSF MOD MENU PKG from the PS3 Package Manager.
2. Open the DSF MOD MENU application from the Games column.
3. Choose INSTALL / UPDATE PLUGIN with CROSS.
4. Reboot the PS3.
5. Launch Driver: San Francisco.
6. Press the PS button briefly to open the in-game overlay.

The installer creates a mode=fetch settings file only if one does not already exist. Fetch mode downloads cheat data but does not submit anonymous votes.

## Remove

Open DSF MOD MENU, choose REMOVE PLUGIN, then reboot. The app removes only its own boot_plugins entry and restores the plugin backup if it created one. Other plugin entries and game files are left untouched.

## Important limitations

This is the upstream Simple Cheat Menu overlay, not a custom recreation of RageX. It does not include RageX's vehicle spawner, traffic hiding or Square-button speed boost.

The user's displayed game version is 1.00. The current BLES00891 community cheat data does not have a confirmed green score for that exact build. Do not activate amber or red cheats if stability is the priority. The package build is not a hardware test and no crash-free guarantee is possible.
