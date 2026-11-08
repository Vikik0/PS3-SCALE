# PS3 SCALE patch trust policy

## Trust tiers

- **Tier A — Hardware verified:** tested on real PS3 hardware for the exact Title ID and update version, with tester and source evidence.
- **Tier B — Community reported:** reported by a community source, but not independently reproduced by PS3 SCALE.
- **Tier C — Emulator converted / unverified:** imported from RPCS3 or another source without real-hardware confirmation.
- **Blocked:** known to freeze, corrupt, mismatch the game version, or have an unresolved safety issue.

The current starter catalogue contains only B_COMMUNITY records. None should be described as verified on the user's console.

## Before a profile can be armed

1. Match the exact PS3 Title ID.
2. Match the exact game APP_VER.
3. Display the effect, community trust level and source reference.
4. Require an explicit user confirmation.
5. Require webMAN MOD and its PS3MAPI endpoint to be available.
6. Accept only a syntactically valid one-write profile in this build.
7. Record the Title ID, APP_VER and exact patch data in the generated script header.

## Runtime guard

- The per-title webMAN script checks the exact Title ID and in-game state.
- /patch.ps3 starts at the requested address with stop=address+0x20 and step=64. Given the current 1-16-byte signature limit and webMAN's 16-byte upper-bound rounding, only the requested address is a candidate. It writes only when the expected original bytes match.
- The script deletes itself after the one launch attempt. It is not a permanent auto-run profile.
- Multi-write records remain disabled for automatic application.
- No success popup is shown because the current batch-script path cannot verify the post-write value back to PS3 SCALE.
- The app checks APP_VER when arming and checks the mounted game's APP_VER before Start Game. The webMAN script itself has no direct APP_VER condition, so do not update a game between arming the script and launching it.

## Reversibility and files

- Profiles write to the running game process memory only. They do not modify the original game folder, EBOOT, ISO or update files.
- Closing the game ends the process and clears its in-memory patch.
- Press SQUARE before launch to remove a pending PS3 SCALE script. If the patch already ran, exit the game to clear the memory change.
- PS3 SCALE never overwrites or deletes a webMAN script that it does not own.
- Tier C profiles are not accepted by the current catalogue parser or installer.
- Do not expose PS3MAPI to the public internet. Keep it on the local trusted network.

A patch described as an FPS unlock is not frame generation and does not guarantee a stable target FPS.
