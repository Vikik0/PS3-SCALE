# Companion tools and safe integration boundary

PS3 SCALE keeps the established PS3 homebrew tools separate, but can prepare selected community NCL files for Artemis after strict game matching.

## Apollo Save Tool

Official project: https://github.com/bucanero/apollo-ps3

Apollo is a separate save-game, trophy and license manager. Its documented PS3 features include listing HDD/USB saves, unlocking copy-protected saves, patching and resigning saves, downloading community saves, trophy backup/restore and license backup/restore. These are save-data operations, not graphics rendering.

Known application path used for local detection:

`/dev_hdd0/game/NP0APOLLO/USRDIR/EBOOT.BIN`

## Artemis PS3

Official project: https://github.com/bucanero/ArtemisPS3

Artemis is a separate cheat-code application. It reads local or USB `.ncl` code files and attaches the selected codes to the matching game process.

PS3 SCALE detects both known package IDs:

- Upstream Artemis: `/dev_hdd0/game/ARTPS3001/USRDIR/EBOOT.BIN`
- Artemis R5 package variant: `/dev_hdd0/game/ARTZ00001/USRDIR/EBOOT.BIN`

The corresponding USERLIST folder is selected automatically. Online DB only installs a file when it is a `GRAPHICS_FPS` NCL entry and the scanned HDD game's Title ID + APP_VER match exactly. Files are downloaded as plain text with a 2 MiB limit and are never overwritten. Temporary downloads are removed the next time PS3 SCALE starts; permanent storage is selectable in Settings.

PS3 SCALE does not execute an NCL file or apply it automatically. The user opens Artemis and selects the downloaded code. NCL files can write to game memory, so their repository source and game-version match must be reviewed before use.

## webMAN MOD

Official project: https://github.com/aldostools/webMAN-MOD

webMAN MOD provides game mounting, a web/FTP interface, temperature information and optional PS3MAPI services. PS3 SCALE uses the local webMAN HTTP endpoint to start games, launch Apollo / Artemis and arm one-launch memory profiles.

Known detection hints:

- `/dev_hdd0/plugins/webftp_server.sprx`
- A line containing `webftp_server.sprx` in `/dev_hdd0/boot_plugins.txt`

A different install path or plugin filename can result in NOT DETECTED even if webMAN is installed. The Companion Tools page labels this as a file hint, not proof that the service is offline. The launch action still tries the bounded local HTTP endpoint when the hint is missing; the live connection result determines whether the request can proceed. The app does not install or update webMAN.

Companion app launch uses the same local HTTP service. A missing plugin-file hint does not block the request; a failed connection is reported by the transport. Normal game launch does not require PS3MAPI. If a one-launch profile was armed but PS3MAPI is unavailable at launch, PS3 SCALE removes its own profile script and attempts to start the game unmodified. If webMAN's local HTTP service itself is unavailable, the app reports the connection failure and remains responsive.

## Why these are separate

- Apollo manages saves, trophies and licenses.
- Artemis loads user-selected NCL code files.
- webMAN handles mounting, web/FTP and optional management interfaces.
- PS3 SCALE scans games, matches exact metadata, downloads eligible community NCL text to Artemis USERLIST, and prepares its own guarded one-launch webMAN profile workflow.

## References

- Apollo Save Tool README: https://github.com/bucanero/apollo-ps3
- Artemis installation and usage: https://github.com/bucanero/ArtemisPS3/blob/master/docs/INSTALLATION.md
- webMAN MOD web commands: https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- PS3MAPI overview: https://consolemods.org/wiki/PS3%3AMAPI
