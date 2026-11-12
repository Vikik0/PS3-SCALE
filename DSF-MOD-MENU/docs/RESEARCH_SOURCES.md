# Research sources and compatibility notes

## Driver: San Francisco PS3 RageX release

- ConsoleCrunch thread: https://consolecrunch.com/threads/release-driver-san-francisco-first-ever-ps3-mod-menu-by-team-ragex.33852/
- The announcement lists hide random traffic/pedestrians, vehicle spawning, and vehicle speed boost on SQUARE. It claims support for CEX/DEX/HEN, Disc/Digital, and all regions, while acknowledging minor issues.
- The post links a third-party installer and VirusTotal page. The binary is not part of this source tree and has not been independently audited here.

## Artemis cheat database

- Codelist: https://ps3.aldostools.org/codelist.html
- The search result lists BLES00891 / Driver San Fransisco / 01.01 / 2.1 KB.
- Community game-cheat source: https://github.com/mohasi/game-cheats/blob/main/source/BLES00891.txt
- The text file lists three unnamed memory codes, Car Health Never Decreases, Infinite WP, and Max Money On Gain. The unnamed codes are intentionally excluded.
- The codelist version is 01.01. The user's XMB screen showed 1.00, so this data is not yet safe to use on their installed build.

## Official update metadata

- Public PS3 update metadata lists BLES00891 patch version 01.01:
  https://github.com/PS3-Pro/Game-Updates/blob/master/np/np/BLES00891/BLES00891-ver.xml
- Public IRD index lists BLES00891 as Driver: San Francisco version 01.01:
  https://ps3.aldostools.org/ird.html
- This does not prove the user's installed package is already updated. Read the installed PARAM.SFO and hash EBOOT.BIN first.

## PS3 modding / loader architecture

- PS3 game modding overview: https://consolemods.org/wiki/PS3:Modding_PS3_Games
  - Describes EBOOT, SPRX and RPF deployment and warns to match CEX/DEX compatibility.
- RouLette VSH Menu: https://github.com/TheRouletteBoi/RouLetteVshMenu
  - Describes loading SPRX modules into a game without EBOOT replacement.
  - Its README warns some games exceed memory limits and may need an EBOOT loader workaround.
  - It documents Cobra 8.3+ as a requirement for its launcher.
- VshFpsCounter architecture: https://github.com/LuanTeles/VshFpsCounter
  - Describes separate VSH-side overlay and game-process sensor PRX modules.
  - Its build instructions require Sony PS3 SDK 4.75+; its README lists incompatible games.
- Dear ImGui PS3 port: https://github.com/DeMaxi1337/dear-imgui-ps3
  - MIT-licensed UI/backend reference.
  - Its current sample Makefile builds a standalone ELF/SELF/PKG, not a complete in-game VSH plugin. The default input hotkey in the backend is L3+R3. It is not a drop-in DSF mod menu.

## PC mods — do not copy to PS3

- ModDB list: https://www.moddb.com/games/driver-san-francisco/mods
- ModDB downloads: https://www.moddb.com/games/driver-san-francisco/downloads
- Includes PC mods such as Felony Rework, Busted, Moods Rework, Demolition Derby, Restored Split Screen, and PC Mod Menu. No verified PS3 ports were established for these entries.

## FPS patch compatibility

- Community real-hardware patch project: https://github.com/DoSpamu/RPCS3toArtemisPatches
- Its indexed Driver: San Francisco entry is BLUS30536 v1.00, not BLES00891. It must not be applied to BLES00891 by guessing offsets.
