# PS3 SCALE

PS3 SCALE is a PS3 homebrew application for managing PS3/PS2 game libraries and title-specific enhancement workflows. It is designed around read-only game discovery, exact title/version matching, reversible PS2 CONFIG operations, and guarded webMAN/PS3MAPI integration.

## Current development snapshot

- Application: **Alpha 0.33**
- Package metadata: `APP_VER=00.33`, `VERSION=00.33`
- Package ID: `PS3SCALE1`
- Development branch: `design/splash-transitions-ui-v6`
- Review: [Draft PR #16](https://github.com/Vikik0/PS3-SCALE/pull/16)

The latest Alpha 0.33 GitHub Actions run #903 passed host tests, the support Edge Function type-check, and PS3 ELF/SELF/standard PKG/GNPDRM PKG validation. A successful CI build is not a substitute for a physical-console smoke test; hardware behavior remains explicitly unverified until tested on a real PS3.

## Current features

### Game library
- Scans installed PS3 titles, PS3 folders/ISOs, PS2 Classics and supported PS2 ISO formats.
- Reads PARAM.SFO and ISO metadata for Title ID, APP_VER and disc serial information.
- Provides separate PS3/PS2 views, search, sorting, favorites, recents and game details.
- Scanning is read-only; original game folders, EBOOT files and ISO images are not modified.

### Online catalogue and community NCL files
- HTTPS catalogue refresh with certificate verification, response-size limits and validated cache replacement.
- Exact Title ID, inferred region and APP_VER matching for profile eligibility.
- Community NCL installation is limited to reviewed `GRAPHICS_FPS` entries and Artemis USERLIST paths.
- Download verification checks indexed byte size and GitHub Git-blob SHA-1 before installation.
- Temporary/permanent NCL storage modes are available.
- NCL files are not executed or automatically applied by PS3 SCALE; users review and select them in Artemis.

### FPS profiles and game launch
- One-launch webMAN profile scripts are restricted to eligible single-write community profiles.
- The app revalidates game identity/version and checks the expected original bytes before attempting a write.
- Multi-write/manual-only profiles are not automatically applied.
- If safe profile arming is unavailable, the app can fall back to an unmodified webMAN launch when the selected game identity is still valid.
- Foreign webMAN scripts are not overwritten or removed.

### PS2 CONFIG Manager
- Builds a restricted in-memory CONFIG preview.
- Supports verified backup, restore, guarded replacement and rollback for supported PS2 ISO sidecars and PS2 Classics CONFIG files.
- Backup/restore operations are bounded and byte-verified.
- Existing backups and rollback files are not silently overwritten.
- The original ISO, EBOOT and installed game files remain read-only.

### Interface and settings
- Controller-operated UI with English, Arabic and Spanish layouts.
- Background workers for scanning and network operations.
- Two-track music controls, volume, animation, language and enhancement-storage preferences.
- Companion launch shortcuts for Apollo Save Tool and Artemis PS3.
- Real Before/After screenshots are shown only when a genuine exact Title ID + APP_VER pair is available.

### Persistence and support reliability — Alpha 0.33
- Support reports are saved as atomic JSON outbox files with stable UUIDs.
- Pending reports survive app restarts and can be retried from Help & Support.
- The public support endpoint reads request bodies with a strict 12 KiB streaming cap and rejects invalid UTF-8 before JSON parsing.
- Help & Support warns users not to include PSN IDs, passwords or account tokens; its form and delivery/error messages are localized in Arabic and Spanish.
- Server responses are mapped to retry guidance; successful sends remove only the matching pending file.
- The dedicated Supabase migration and Edge Function are prepared, but live reporting stays disabled until a separate Supabase Free Plan organization is created, the isolated project is deployed, and its publishable key/URL are configured. Do not create it under the existing VIKIK Pro organization.

### Persistence and download safety — Alpha 0.33
- Settings and favorites are saved through unique exclusive temporary files, fsync and atomic rename.
- One-launch webMAN scripts refuse existing temporary files and symlink paths.
- HTTPS downloads use unique O_EXCL temporary names; stale files do not block future attempts.
- Failed downloads do not delete pre-existing destination files.
- Host regression tests cover atomic writes, symlink safety and download-failure cleanup.

## Safety and known limitations

- Community profiles and NCL files are not independently hardware-verified on every PS3 model.
- Git-blob SHA-1 is an integrity check, not a cryptographic signature. Signed catalogue/update manifests remain future work.
- There is no universal AI upscaler, frame generation, or guaranteed 60 FPS.
- Genuine Before/After screenshot pairs are not bundled for every title; the app does not substitute concept art for missing captures.
- The online support-report endpoint is intentionally disabled until a dedicated PS3 SCALE project is deployed inside a separate Supabase Free Plan organization. The connected account currently exposes only VIKIK (Pro), so no project has been created. Reports can still be saved to the console's local outbox. Existing unrelated projects must not be reused.
- Physical PS3 validation is still required for installation, UI navigation, music, webMAN/PS3MAPI behavior, Artemis NCL loading, PS2 CONFIG behavior and real game stability.

## Build and test

Build environment: PS3DEV / PSL1GHT / Tiny3D and the dependencies listed in the Makefile.

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make clean
make pkg
```

Run host-side regression tests:

```sh
make host-test
# or
./scripts/test_host.sh
```

For installation, download the latest successful **GNPDRM PKG** artifact from [GitHub Actions](https://github.com/Vikik0/PS3-SCALE/actions). Test on a compatible CFW/HEN console before treating any hardware-dependent feature as complete.

## Documentation

- [Arabic setup and feature guide](PS3-SCALE-Alpha-0.2/README_AR.md)
- [UI flow](PS3-SCALE-Alpha-0.2/docs/UI_FLOW.md)
- [Game enhancement lifecycle](PS3-SCALE-Alpha-0.2/docs/GAME_ENHANCEMENT_WORKFLOW.md)
- [Online database and NCL safety](PS3-SCALE-Alpha-0.2/docs/ONLINE_DATABASE.md)
- [PS2 CONFIG engine](PS3-SCALE-Alpha-0.2/docs/PS2_ENGINE.md)
- [Device test matrix](PS3-SCALE-Alpha-0.2/docs/TEST_MATRIX.md)
- [Roadmap](PS3-SCALE-Alpha-0.2/docs/ROADMAP.md)
- [Build status](PS3-SCALE-Alpha-0.2/docs/BUILD_STATUS.md)
- [Dedicated support backend setup](supabase/README.md)
