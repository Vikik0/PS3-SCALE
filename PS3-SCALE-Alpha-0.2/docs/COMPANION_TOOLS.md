# Companion tools and safe integration boundary

PS3 SCALE focuses on game-library discovery, metadata, and future verified per-title profiles. It does not replace the established PS3 homebrew tools below.

## Apollo Save Tool

Official project: https://github.com/bucanero/apollo-ps3

Apollo is a separate save-game, trophy and license manager. Its documented PS3 features include listing HDD/USB saves, unlocking copy-protected saves, patching and resigning saves, downloading community saves, trophy backup/restore and license backup/restore. These are save-data operations, not graphics rendering.

Known application path used for local detection:
\`/dev_hdd0/game/NP0APOLLO/USRDIR/EBOOT.BIN\`

## Artemis PS3

Official project: https://github.com/bucanero/ArtemisPS3

Artemis is a separate cheat-code application. Its documentation describes local or USB \`.ncl\` code files and a runtime plugin/engine. A game-specific code is selected in Artemis, then the Artemis runtime attaches to the matching game process. This is not a universal graphics upscaler.

Known application path used for local detection:
\`/dev_hdd0/game/ARTPS3001/USRDIR/EBOOT.BIN\`

## webMAN MOD

Official project: https://github.com/aldostools/webMAN-MOD

webMAN MOD provides game mounting, a web/FTP interface, temperature information and optional PS3MAPI services. PS3MAPI can expose process information and memory operations; memory writes are high risk and are not part of Alpha 0.2.

Known detection hints:
- \`/dev_hdd0/plugins/webftp_server.sprx\`
- A line containing \`webftp_server.sprx\` in \`/dev_hdd0/boot_plugins.txt\`

A different install path or plugin filename can result in NOT DETECTED even if webMAN is installed. The Companion Tools page is a best-effort local file check; it does not contact webMAN, launch any companion application, load a PRX, or issue PS3MAPI commands.

## Why these are separate

- Apollo manages saves, trophies and licenses.
- Artemis attaches title-specific cheat codes through its own runtime.
- webMAN handles mounting, web/FTP and optional management interfaces.
- PS3 SCALE currently scans games and prepares profile metadata only.

Future integrations must begin read-only, remain on the local console/LAN, verify exact game identity, and never write to game memory or files without an explicit backup and rollback path.

## References

- Apollo Save Tool README: https://github.com/bucanero/apollo-ps3
- Artemis installation and usage: https://github.com/bucanero/ArtemisPS3/blob/master/docs/INSTALLATION.md
- webMAN MOD web commands: https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- PS3MAPI overview: https://consolemods.org/wiki/PS3%3AMAPI
