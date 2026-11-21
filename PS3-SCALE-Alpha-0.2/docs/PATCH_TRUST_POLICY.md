# PS3 SCALE patch trust policy

## Trust tiers

- **Tier A — Hardware verified:** tested on real PS3 hardware for the exact Title ID and update version, with tester and source evidence.
- **Tier B — Community reported:** reported by a community source, but not independently reproduced by PS3 SCALE. B_COMMUNITY_MANUAL remains Tier B but is explicitly excluded from one-launch automation when the source requires in-game timing or reapplication.
- **Tier C — Emulator converted / unverified:** imported from RPCS3 or another source without real-hardware confirmation.
- **Blocked:** known to freeze, corrupt, mismatch the game version, or have an unresolved safety issue.

The current starter memory-profile catalogue contains 33 one-launch-eligible B_COMMUNITY records, 6 multi-write B_COMMUNITY records disabled for automation, and 1 B_COMMUNITY_MANUAL record. None should be described as verified on the user's console.

## Before a memory profile can be armed

1. Match the exact PS3 Title ID.
2. Match the exact game APP_VER.
3. Display the effect, community trust level and source reference.
4. Require an explicit user confirmation.
5. Require webMAN MOD and its PS3MAPI endpoint to be available before arming.
6. Accept only a syntactically valid one-write profile in this build.
7. Record the Title ID, APP_VER and exact patch data in the generated script header.

## Runtime guard

- The per-title webMAN script checks the exact Title ID and in-game state.
- `/patch.ps3` starts at the requested address with stop=address+0x20 and step=64. Given the current 1–16-byte signature limit and webMAN's 16-byte upper-bound rounding, only the requested address is a candidate. It writes only when the expected original bytes match.
- The complete V2 script body is compared with the deterministic generated content before Start Game. A matching header alone is insufficient; edited or appended commands are treated as stale and must be removed or regenerated.
- The script deletes itself after the one launch attempt. It is not a permanent auto-run profile.
- If PS3MAPI is unavailable at launch, PS3 SCALE removes its own one-launch script and attempts to start the game unmodified. Normal launch does not require PS3MAPI; it does require the local webMAN HTTP service.
- No success popup is shown because the current batch-script path cannot verify the post-write value back to PS3 SCALE.
- The app checks APP_VER when arming and checks the mounted game's APP_VER before Start Game. The webMAN script itself has no direct APP_VER condition, so do not update a game between arming the script and launching it.

## Community NCL file installer

The Online DB Cross action is a separate file-transfer workflow, not the one-launch memory-profile installer.

- Only entries tagged `GRAPHICS_FPS` with a `.ncl` extension are eligible.
- The local PS3 game must be scanned from `/dev_hdd0/`, and Title ID + APP_VER must match the index entry exactly.
- Artemis PS3 must be detected. The installer supports the upstream `ARTPS3001` and Artemis R5 `ARTZ00001` package IDs.
- The source path is validated; downloads are limited to 2 MiB and must be plain text. ELF, PKG, binary content and unsupported file types are rejected.
- Existing USERLIST files are never overwritten.
- The NCL is copied into the detected Artemis `USRDIR/USERLIST/` directory. It is not copied over the original game folder or EBOOT.
- PS3 SCALE does not execute or auto-apply the NCL. The user opens Artemis and selects it. NCL files can contain memory-writing codes, so the source and version must be reviewed.
- Temporary storage is the default and is removed on the next PS3 SCALE start. Permanent storage is selectable in Settings.

The category and exact version match are filters, not proof that a community NCL is safe or hardware-tested. Tier C and other community-supplied NCL entries may still be present in the online index; review their source before using them.

## Reversibility and files

- Memory profiles write to the running game process only. They do not modify the original game folder, EBOOT, ISO or update files.
- Closing the game ends the process and clears its in-memory patch.
- Press SQUARE before launch to remove a pending PS3 SCALE memory script. If the patch already ran, exit the game to clear the memory change.
- PS3 SCALE never overwrites or deletes a webMAN script that it does not own.
- Artemis NCL files are separate files in Artemis USERLIST. Temporary files are tracked by PS3 SCALE and deleted at the next PS3 SCALE startup; permanent files must be removed manually.
- Do not expose PS3MAPI to the public internet. Keep it on the local trusted network.

A patch described as an FPS unlock is not frame generation and does not guarantee a stable target FPS.
