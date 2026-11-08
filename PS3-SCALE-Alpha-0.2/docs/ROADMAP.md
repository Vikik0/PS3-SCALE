# PS3 SCALE roadmap

## Alpha 0.1 — safe library foundation (host-tested)

- [x] PS3 PARAM.SFO parser
- [x] Folder game scanner (`GAMES`, `GAMEZ`)
- [x] Installed-title scanner (`HG`, `2P`, `2G`)
- [x] PS3 ISO / PS2 ISO filename discovery
- [x] PS2 ISO9660 `SYSTEM.CNF` serial probe
- [x] PS3 Title ID hint extraction from common ISO filenames
- [x] Direct mounted Blu-ray scan path implemented
- [x] Internal HDD, Blu-ray mount, USB000..USB007 scan paths
- [x] Read-only scan behavior
- [x] Build with maintained Tiny3D PS3 toolchain container
- [ ] Test on CECH-2504A / CFW 4.93

## Alpha 0.2 — library and profile UX
- ICON0/PIC1 extraction
- User profile storage under `/dev_hdd0/game/PS3SCALE1/USRDIR/`
- Per-title compatibility status
- Local backup/restore manifest (no automatic game-file edits)

## Alpha 0.3 — PS2 configuration manager
- [x] Add a restricted in-memory PS2 netemu CONFIG builder for commands 0x44/0x46, with host tests\n- PS2 Classics/PS2 ISO metadata enrichment
- Match known `ps2_netemu` CONFIG files by title ID and region
- Hash verification and backup/restore
- No automatic config writes until hardware tests pass

## Alpha 0.4 — PS3 title-specific patch manager
- Online catalog with signed manifest and SHA-256 checks
- Exact title ID + update version matching
- Patch preview, backup, apply, rollback
- First supported title only after repeatable hardware test

## Later research
- [x] Document optional webMAN MOD / PS3MAPI route and read-only-first safety boundary\n- CFW PRX/per-title runtime hook feasibility
- RSX resource/memory budget
- Game-specific framebuffer or shader experiments
- Frame interpolation feasibility and performance cost

A release is not considered tested until a PKG is built, installed, launched, and the scanner is
verified on physical hardware. The host SFO tests do not substitute for console testing.


## Alpha 0.2 — build pipeline and metadata hardening
- [x] Replace incomplete Makefile with PSL1GHT recursive build layout
- [x] Use maintained Tiny3D toolchain container in GitHub Actions
- [x] Add PS2 ISO9660 serial metadata probe
- [x] Preserve multiple copies of one Title ID on separate drives
- [x] Add developer research and online catalog safety rules
- [x] Execute GitHub Actions on the user's repository (Run #15 passed)
- [x] Inspect resulting PKG and SELF; finalized PKG header and Content ID validated
- [ ] Install and test on physical CECH-2504A


## Build milestone — 8 Nov 2026
- [x] Host SFO, scanner and ISO tests pass in GitHub Actions.
- [x] PPU ELF and CEX SELF built with the Tiny3D toolchain.
- [x] Debug PKG and finalized NPDRM PKG generated.
- [x] Finalized PKG magic, revision (0x8000), PS3 package type and declared file size validated.
- [x] Artifacts uploaded: Run #8, artifact ID `13616618869`.
- [ ] Install, launch and test on the physical CECH-2504A.
- [ ] Confirm game scan results against the user's actual HDD and USB layout.
