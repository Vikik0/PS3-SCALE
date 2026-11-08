# PS3 SCALE

PS3 SCALE is a homebrew project for safely cataloguing PS3 and PS2 titles and, in later releases, managing verified per-title graphics and compatibility enhancements.

## Current status — Alpha 0.2

- Read-only PS3/PS2 game discovery and metadata parsing.
- Tiny3D-based PS3 application UI.
- PS3 folder, installed-title, PS3 ISO and PS2 ISO discovery.
- PARAM.SFO parsing and PS2 ISO9660 SYSTEM.CNF serial extraction.
- GitHub Actions builds ELF, SELF, debug PKG and finalized NPDRM PKG.
- Host-side tests pass.
- Physical PS3 installation and runtime validation are still pending.

**Important:** Alpha 0.2 does not apply game patches or provide a universal in-game upscaler. Real enhancement work must be title-specific and verified against the exact game ID/version. Original game files remain read-only.

## Build and install

See [the latest successful build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42480769127) and download the PS3-SCALE-Alpha-0.2-build artifact. Use ps3scale.gnpdrm.pkg for the finalized package.

## Documentation

- [Arabic setup and install notes](PS3-SCALE-Alpha-0.2/README_AR.md)
- [Roadmap](PS3-SCALE-Alpha-0.2/docs/ROADMAP.md)
- [Verified build report](PS3-SCALE-Alpha-0.2/docs/BUILD_STATUS.md)
- [Graphics engine feasibility](PS3-SCALE-Alpha-0.2/docs/GRAPHICS_ENGINE_REALITY.md)
- [PS2 CONFIG route](PS3-SCALE-Alpha-0.2/docs/PS2_ENGINE.md)\n- [Optional webMAN MOD / PS3MAPI integration](PS3-SCALE-Alpha-0.2/docs/WEBMAN_MAPI_INTEGRATION.md)
- [Patch trust and rollback policy](PS3-SCALE-Alpha-0.2/docs/PATCH_TRUST_POLICY.md)
- [Developer research notes](PS3-SCALE-Alpha-0.2/docs/RESEARCH_NOTES.md)

## First hardware target

CECH-2504A Slim · CEX CFW 4.93 · Cobra 8.5.

The application does not require PSN.
