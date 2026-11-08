# Graphics engine: engineering boundary and implementation path

## What a normal GameOS app can do
A normal PSL1GHT application can scan files, parse game metadata, manage per-title profiles,
download verified files, and prepare reversible title-specific changes.

## What it cannot do by itself
When the user launches a game from XMB, the manager app is no longer the process rendering
that game's frames. Therefore a standalone manager cannot simply place a universal post-process
shader over every game's final framebuffer.

A real in-game enhancement path needs one of:
1. A tested title-specific EBOOT/game patch (resolution, frame pacing, widescreen, etc.).
2. A CFW PRX/plugin or per-title hook that runs inside the game process and is validated for
   the exact title ID, game update, and firmware.
3. For PS2 Classics, a compatible `ps2_netemu` CONFIG file where the emulator supports it.

## No fake switches
The Alpha 0.1 UI intentionally does not expose a working "AI Upscale" or "Frame Generation"
toggle. Those controls must remain disabled until a real backend exists and has been measured
on hardware. Sharpening a menu screenshot or changing HDMI output resolution is not proof that
the rendered game resolution increased.

## PS3 Slim test target
The first physical target is CECH-2504A, CEX CFW 4.93, Cobra 8.5. The exact graphics backend
must be validated on this hardware before enabling any patch for other Slim/FAT/Super Slim units.


## Optional PS3MAPI backend research

webMAN MOD can expose PS3MAPI over HTTP and supports process information plus memory access. This gives PS3 SCALE a possible per-title live-memory patch route on CEX, but it does not provide a universal shader or framebuffer post-process layer.

Alpha 0.2 must remain read-only. A future implementation should first query the current process and title identity, then require an exact game/version match and verified original bytes before any memory write. The MAPI service must remain on the trusted local network; IDPS/PSID access, syscall changes and clock changes are out of scope.

See [WEBMAN_MAPI_INTEGRATION.md](WEBMAN_MAPI_INTEGRATION.md).
