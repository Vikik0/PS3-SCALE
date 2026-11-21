
# Research sources

## Game version and regional identity

- PS3 title/update listing with Ghosts TU 1.16 and regional Title IDs: https://ps3.aldostools.org/updates.html
- Ghosts PS3 modding guide with TU 1.16, title-ID and CEX/DEX compatibility notes: https://github.com/richard1222/ghosts-ps3-mod-menus
- Official PS3 instruction manual: https://www.callofduty.com/content/dam/atvi/callofduty/ghosts/manuals/Ghosts-Manual-PS3-en.pdf

## Local multiplayer behavior

- PS3 title-update notes mention a split-screen UI fix: https://www.playstationtrophies.org/news/news-11737-call-of-duty-ghosts-updated-on-ps3-ps4.html
- PS3 GameFAQs Q&A reports Multiplayer, Squads and Extinction available offline with one or two local players: https://gamefaqs.gamespot.com/ps3/703505-call-of-duty-ghosts/answers/361457-can-the-multiplayer-played-offline
- MobyGames PS3 technical specs list DualShock 3, same/split-screen and 1–2 offline players: https://www.mobygames.com/game/64165/call-of-duty-ghosts/specs/ps3/
- Campaign single-player status and distinction from Extinction co-op: https://codghost.net/faq/

These are research leads; local-player behavior must be verified against the exact PS3 release and update.

## PS3 controller input API

- PS3DK `cell/pad.h` declarations and controller data layout: https://github.com/FirebirdTA01/PS3DK/blob/v0.27.0/sdk/include/cell/pad.h
- PS3DK `libio_stub.yaml` system import list for `cellPadGetInfo2` and `cellPadGetData`: https://github.com/FirebirdTA01/PS3DK/blob/v0.27.0/tools/nidgen/nids/extracted/libio_stub.yaml
- PS3 Developer Wiki sys_io export list: https://playstationdev.wiki/ps3/VSH_Exports

The adapter reads ports 0 and 1 only and does not call `cellPadInit`/`cellPadEnd`, because it is intended to run inside an already active game process. Runtime behavior remains unverified until tested on the target console.

## IW6 script and FastFile research

- GSC Tool — documents IW6 (Ghosts) script compile/decompile support for PS3: https://github.com/xensik/gsc-tool
- CoD Research — Ghosts PS3 FastFile format and signed-container limitations: https://github.com/primetime43/CoD-Research/blob/main/Ghosts-COD10/FastFiles/FastFiles.md
- CoD Research — Ghosts PS3 zone layout and scriptfile asset extraction notes: https://github.com/primetime43/CoD-Research/blob/main/Ghosts-COD10/FastFiles/Zone.md
- IW6 FastFile research hub: https://codresearch.dev/index.php/FastFiles_and_Zone_files_%28Ghosts%29

## PS3 runtime and candidate functions

- Ghosts PS3 TU 1.16 offsets/functions collection, including `PlayerCMD_IsSplitScreenPlayer` and `PlayerCMD_IsSplitScreenPlayerPrimary`: https://nextgenupdate.com/forums/ghosts-mods-cheats-glitches/901435-ghosts-offsets-addresses-collection-116updating-3.html
- PS3 homebrew SDK/toolchain: https://github.com/ps3dev/PSL1GHT
- PS3 game modding overview: https://consolemods.org/wiki/PS3:Modding_PS3_Games

Research notes do not establish that any offset is valid for Campaign or that a candidate hook is safe. The exact executable identity and runtime behavior must be verified before use.
