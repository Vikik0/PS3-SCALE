# Graphics engine: engineering boundary and implementation path

## What a normal GameOS app can do
A normal PSL1GHT application can scan files, parse game metadata, manage per-title profiles,
download verified files, and prepare reversible title-specific changes.

## What it cannot do by itself
When the user launches a game from XMB, the manager app is no longer the process rendering
that game's frames. Therefore a standalone manager cannot simply place a universal post-process
shader over every game's final framebuffer.

The practical PS3 path is title-specific, not a global neural filter:
1. Read the exact Title ID and `APP_VER` from the mounted `PARAM.SFO`.
2. Match a real-hardware-tested profile for that exact title/update.
3. For live memory patches, use webMAN MOD / PS3MAPI or its per-title game-load batch hook (`/dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat`). The script can apply a known patch and show a VSH popup only after the write is confirmed.
4. For file-based patches, create and verify a backup before changing an EBOOT; keep the original game data restorable.
5. For PS2 Classics, use a compatible `ps2_netemu` CONFIG only where the game/emulator supports it.

## No fake switches
The GNM API and the neural-upscaling/frame-generation concept in the reference screenshot belong to the PS4 graphics ecosystem, not the PS3. PS3 uses the Cell processor and NVIDIA RSX with the older PS3 graphics stack; GNM code cannot be dropped into a PS3 title. The RSX has 256 MB of local GDDR3, so forcing a higher internal resolution can increase framebuffer cost and reduce performance. Sharpening a menu screenshot or changing HDMI output resolution is not proof that the rendered game resolution increased. No universal AI upscaling or frame generation is promised; only measured per-title improvements can be enabled.

## PS3 Slim test target
The current test target is a PS3 Slim running CFW 4.92. The exact CECH model and Cobra version
have not yet been confirmed, so the app must read the detected hardware/firmware before enabling
any title-specific profile. No profile should be assumed compatible across FAT, Slim and Super Slim units.


## Optional PS3MAPI backend research

webMAN MOD can expose PS3MAPI over HTTP and supports process information plus memory access. This gives PS3 SCALE a possible per-title live-memory patch route on CEX, but it does not provide a universal shader or framebuffer post-process layer.

The current Start Game button only launches the mounted title; it does not apply a graphics patch. A future implementation should first query the current process and title identity, then require an exact game/version match and verified expected bytes before any memory write. webMAN supports a per-game batch script at `/dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat`; this is the first runtime route to test. The MAPI service must remain local; IDPS/PSID access, syscall changes and clock changes are out of scope.

See [WEBMAN_MAPI_INTEGRATION.md](WEBMAN_MAPI_INTEGRATION.md).
