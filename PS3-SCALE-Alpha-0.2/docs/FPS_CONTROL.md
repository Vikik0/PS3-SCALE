# PS3 SCALE — FPS Control

## Scope

FPS Control exposes only the game-specific performance profiles already present in the PS3 SCALE catalogue. It is not a universal frame-rate slider and does not synthesize a patch for an unsupported game.

This screen is separate from the Online DB NCL installer:
- FPS Control arms a PS3 SCALE one-launch webMAN memory profile.
- Online DB can install an exact-match community NCL text file into Artemis USERLIST for the user to select in Artemis.

## User flow

1. Open Scan Games and select a PS3 title.
2. Open Game Details, then press SQUARE for FPS Control.
3. Use UP/DOWN to select Original or the exact catalogue profile.
4. CROSS prepares the selected profile. TRIANGLE confirms the one-launch install.
5. SQUARE removes a PS3 SCALE-owned pending script. CIRCLE cancels confirmation or returns to Game Details.
6. Start the game from Game Details after the profile has been armed.

For the normal launch path, press CROSS or START directly in Game Details. PS3 SCALE treats that button press as explicit consent, arms an eligible exact one-write B_COMMUNITY profile, and then launches the game. If no exact profile exists, the original game starts unmodified. Manual-only and multi-write profiles remain blocked.

## Matching and safety

- A profile is exposed only for an exact Title ID + APP_VER match.
- The current catalogue uses community-reported entries. These are not hardware-verified by PS3 SCALE.
- Only one-write `B_COMMUNITY` profiles are eligible for automatic one-launch setup in this build.
- Multi-write and `B_COMMUNITY_MANUAL` entries remain disabled for automatic application.
- webMAN MOD and PS3MAPI must respond before a memory profile can be armed.
- Normal game launch requires webMAN's local HTTP service, not PS3MAPI.
- If PS3MAPI is unavailable after a profile was armed, PS3 SCALE removes its own one-launch script and still attempts to start the game without the profile. If webMAN itself is unavailable, the app reports the local HTTP/network failure.
- PS3 SCALE protects foreign webMAN scripts and does not overwrite or delete them.
- The patch is written to the running game's memory only. It does not modify ISO, EBOOT or installed game files; closing the game ends the patched process.

## Frame-rate limitations

- The catalogue's effect label (for example, `60 FPS` or `Uncapped FPS`) describes the reported patch, not a measured result.
- Unlocking a cap does not guarantee that the PS3 can render at the requested rate.
- Some games tie physics, animation, timing or game speed to frame rate. An FPS patch can therefore cause instability or incorrect game speed.
- There is no generic 30/60/custom slider because changing an FPS target requires a verified game-specific patch.
- This version does not provide a trusted live FPS counter or post-write verification of the memory value.

## Required hardware validation

For every profile promoted to hardware-tested status, record:
- PS3 model family, CFW version and webMAN MOD version.
- Exact Title ID and APP_VER.
- Original behavior before patching.
- Whether the patch is applied at the intended address and whether the expected bytes matched.
- Actual in-game frame-rate measurements across a repeatable scene.
- Stability, physics/game-speed behavior, and any regressions.
- Confirmation that exiting the game restores the original process state.

Until those checks are complete, keep the profile marked community-reported and show the warning in the UI.
