# DSF MOD MENU build gates

## Why no installable menu PKG is emitted yet

The PS3 SCALE repository has a working PSL1GHT/Tiny3D application build pipeline, but a standalone XMB application is not an in-game overlay. A game overlay needs a VSH-side rendering/input component and a separate, title-specific game-process component or a carefully bounded MAPI backend.

The available Dear ImGui PS3 example builds a standalone PKG; it does not by itself provide the required VSH plugin and game injection. The existing PS3 SCALE build workflow must not be repurposed to claim an in-game plugin merely because it can produce a PKG.

## Required implementation sequence

1. Confirm the exact target executable:
   - PARAM.SFO TITLE_ID must equal BLES00891.
   - Record PARAM.SFO APP_VER.
   - Record EBOOT.BIN SHA-256.
2. Implement the VSH overlay:
   - PAF/VSH-compatible plugin build.
   - D-Pad UP + SELECT edge-triggered toggle.
   - No controller input leakage while the menu is open.
   - Restore all changed render state after drawing.
3. Implement game detection:
   - Only attach to BLES00891.
   - Refuse unknown versions and unknown executable hashes.
   - Avoid process scanning loops that assume unverified PS3MAPI structure layouts.
4. Implement one feature at a time:
   - Start with read-only overlay and diagnostics.
   - Add reversible, version-specific patches only after original-byte validation.
   - Do not import PC mod files into PS3.
   - Do not apply BLUS30536 patches to BLES00891.
5. Installer:
   - Install only under a DSF-owned directory.
   - Add a single boot_plugins.txt line without duplicating existing entries.
   - Preserve the original boot_plugins.txt and provide an uninstaller.
   - Never overwrite EBOOT.BIN or PARAM.SFO.
6. Hardware test:
   - Target CECH-2504A, CEX 4.93, Cobra 8.5.
   - Cold boot with plugin enabled.
   - XMB test, game launch test, overlay open/close test.
   - Story Mode feature-by-feature test.
   - Exit game and reboot test.
   - Uninstall test.
   - Keep the game offline during development.

## Acceptance criteria

A PKG may be labelled **test build** only when:
- ELF/PRX build succeeds.
- PKG structure and metadata validate.
- Installer and uninstaller scripts pass host tests.
- No game files are modified.
- The plugin is verified to load on the target CFW.

A PKG may be labelled **hardware-tested** only after the actual target PS3 passes the full checklist above.

A claim of "100% crash-free" is not technically supportable. The release should instead publish exact tested firmware, title ID, APP_VER, EBOOT hash, known issues, and rollback steps.
