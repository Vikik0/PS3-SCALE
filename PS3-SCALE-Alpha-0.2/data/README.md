# Enhancement catalog data

`catalog.example.json` is intentionally empty. It is a schema example, not a claim that any game patch is currently supported.

`catalog.schema.json` defines the required metadata for future records:
- Exact platform and title identity (PS3 Title ID or PS2 disc serial).
- Region and game/update version.
- Patch type and application method.
- Source, author, license, SHA-256 and exact byte size.
- Confidence tier, hardware test evidence and known issues.
- Mandatory backup and rollback availability.

## Safety rules

- Do not add a real patch record until its source and license are known.
- Do not use a title name alone to select a patch.
- A PS3 record must match the exact nine-character Title ID and game update.
- A PS2 record must match the exact disc serial and emulator/config target.
- PS2 netemu CONFIG files must not exceed 16,384 bytes.
- No Tier C/unverified patch is auto-applied.
- No download is executed as code.
- Alpha 0.2 does not read this catalog or apply any patch yet.

The catalog loader, signature verification and hardware-tested patch application are future milestones.


## Startup music

The user-supplied MP3 is currently loaded from the PS3 filesystem at runtime, not embedded into the PKG. The app searches the paths documented in `docs/AUDIO_THEME.md`; the preferred location is:

`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

Copy the provided `GTA V - Greyhound Pause Menu Theme Remix.mp3` to that path and rename it `theme.mp3`. The player streams it offline, loops it, and exposes mute/volume controls in Settings.
