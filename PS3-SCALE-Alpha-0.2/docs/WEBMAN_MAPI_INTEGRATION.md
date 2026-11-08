# Optional webMAN MOD / PS3MAPI integration

## Purpose

PS3 SCALE uses webMAN MOD as an optional bridge for title-specific, in-memory PS3 enhancements. This is not a universal upscaler. The library scanner and the rest of the app continue to work without webMAN.

## Implemented stages

### Stage 1 — read-only connection check

- Detect whether the local webMAN MOD HTTP endpoint responds.
- Check the PS3MAPI server-version endpoint.
- Do not read console identifiers (IDPS/PSID), alter syscalls, change clocks or write memory during connection detection.

### Stage 2 — exact profile discovery

- Load the bundled or HTTPS-synchronized catalogue.
- Match exact Title ID and APP_VER.
- Display the community trust level, reported effect, source and patch count.
- Require the exact game version to be mounted at /dev_bdvd before arming; the mounted Title ID and APP_VER must match the selected library entry. Explicit controller confirmation is still required.
- Multi-write profiles remain disabled for automatic application in this build. Profiles tagged B_COMMUNITY_MANUAL are also refused by the installer when the source requires an in-game timing/reapply step.

### Stage 3 — one-launch guarded memory profile

- PS3 SCALE creates /dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat only after confirmation.
- The script is tagged with a PS3 SCALE V2 header containing Title ID, APP_VER and exact patch data. Before allowing Start Game, PS3 SCALE compares the complete script file with the deterministic expected content; edited/appended commands are stale even if the header remains unchanged. Legacy V1 scripts are detected as stale and can be removed/replaced.
- On game start, webMAN checks the Title ID and in-game state.
- /patch.ps3 is called with addr=<address>, stop=<address+0x20>, step=64, find=<expected bytes>, rep=1 and val=<replacement bytes>. With the current 1-16-byte signature limit, this leaves only the requested address as a candidate despite webMAN's 16-byte upper-bound rounding, and requires the original bytes to match.
- The script deletes itself after the launch attempt. It does not persist as a permanent per-title auto-run patch.
- No beep or success popup is shown. The current batch-script route cannot verify the post-write memory value back to PS3 SCALE, so no success message is fabricated.
- The memory change belongs to the running game process and is cleared when that game process exits. Original game files remain unchanged.

## Known limitation

The webMAN batch command language exposes Title ID and in-game conditions, but not a direct game APP_VER condition. PS3 SCALE checks APP_VER before arming and checks the mounted APP_VER again when Start Game is used. The script also checks the exact expected bytes at the exact address and is one-launch only. Do not update the game between arming and launching. A CFW PRX or another runtime verifier would be required for a stronger in-game version check and verified success notification.

## Security rules

- Keep PS3MAPI restricted to the local trusted network; never expose it to the public internet.
- webMAN MOD remains optional for scanning and catalogue browsing.
- Never use PS3 SCALE to fetch, display, store or modify IDPS/PSID.
- Never disable syscalls, spoof console identity, change fan settings, or change CPU/RSX clocks as part of a graphics profile.
- Community reports are not hardware verification. The starter catalogue is Tier B only.
- Do not apply a profile with a Title ID, APP_VER, address or expected-byte mismatch.

## Test requirements

1. Confirm the user's webMAN MOD version and that PS3MAPI is enabled.
2. Test read-only connection on the target PS3 Slim / CFW 4.92.
3. Verify the current process/title identity while at XMB and while a game is running.
4. Test one harmless, reversible, single-write profile with a known expected byte at the exact address.
5. Confirm that closing the game clears the process-memory change.
6. Record the before/after behavior and stability on the physical console.

## References

- webMAN MOD Web Commands: https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- webMAN MOD project: https://github.com/aldostools/webMAN-MOD
- PS3MAPI overview: https://consolemods.org/wiki/PS3%3AMAPI
