# PS3 SCALE

PS3 SCALE is a PS3 homebrew application for managing PS3/PS2 game libraries and title-specific enhancement profiles.

## Current status — Alpha 0.2

The main branch has a successful embedded-theme build. The development branch **feature/online-graphics-database** adds:

- A 40-entry starter catalogue of community-reported PS3 profiles.
- HTTPS catalogue synchronization with certificate verification, a download-size cap and validated cache replacement.
- Exact Title ID + APP_VER matching.
- A guarded, one-launch webMAN MOD script for single-write profiles, requiring explicit confirmation.
- Exact-address/original-byte checks, script fingerprinting, stale-script handling and self-removal after the launch attempt.
- Multi-write profiles remain disabled for automatic application.

**Safety status:** the catalogue entries are community-reported, not independently verified on the user's PS3. The webMAN batch path cannot report a verified post-write memory value, so PS3 SCALE does not show a success popup. The patch changes the running game's memory only; original game files remain unchanged. Exiting the game clears the process-memory change.

The feature branch must pass host tests and the PS3 package build, then receive a physical-console smoke test, before it is considered ready for normal use. There is no universal AI upscaler or frame generation.

## Latest verified main build

[GitHub Actions Run #162 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42524757054)

This build is from main and **does not yet include** the online graphics-profile branch. Wait for the feature-branch CI result before installing a build that includes the online profile work.

## Build and install

See the workflow run above, open **Artifacts**, and download the finalized PS3 package artifact. Use **ps3scale.gnpdrm.pkg** for installation on a compatible CFW/HEN console.

## Documentation

- [Arabic setup and install notes](PS3-SCALE-Alpha-0.2/README_AR.md)
- [UI flow](PS3-SCALE-Alpha-0.2/docs/UI_FLOW.md)
- [Game enhancement lifecycle](PS3-SCALE-Alpha-0.2/docs/GAME_ENHANCEMENT_WORKFLOW.md)
- [Online profile catalogue](PS3-SCALE-Alpha-0.2/docs/ONLINE_DATABASE.md)
- [Patch trust policy](PS3-SCALE-Alpha-0.2/docs/PATCH_TRUST_POLICY.md)
- [Optional webMAN MOD / PS3MAPI integration](PS3-SCALE-Alpha-0.2/docs/WEBMAN_MAPI_INTEGRATION.md)
- [Startup theme audio](PS3-SCALE-Alpha-0.2/docs/AUDIO_THEME.md)
- [Roadmap](PS3-SCALE-Alpha-0.2/docs/ROADMAP.md)
- [Build status](PS3-SCALE-Alpha-0.2/docs/BUILD_STATUS.md)
- [Graphics engine feasibility](PS3-SCALE-Alpha-0.2/docs/GRAPHICS_ENGINE_REALITY.md)
- [PS2 CONFIG route](PS3-SCALE-Alpha-0.2/docs/PS2_ENGINE.md)
- [Developer research notes](PS3-SCALE-Alpha-0.2/docs/RESEARCH_NOTES.md)

## Hardware target

Primary target: PS3 Slim, CFW 4.92. The exact CECH model, CEX/DEX type and Cobra version are not yet confirmed for the user's console. FAT and Super Slim compatibility remains a broader project target and must be checked on real hardware.
