# Build status — PS3 SCALE Alpha 0.36

## Current development head

- Branch: `feature/ps2-enhancement-engine`
- Pull request: Draft PR #20, targeting `design/splash-transitions-ui-v6`
- Application version shown in the UI: `ALPHA 0.36`
- Package metadata: `APP_VER=00.36`, `VERSION=00.36`
- Package title ID: `PS3SCALE1`
- Source commit used for the Alpha 0.35 package: `261da5906c68bf3fbf406efe9a669ec7b9388e77`

## Alpha 0.36 — USB CONFIG cache routing and explicit netemu launch

- USB PS2 ISO CONFIG sidecars now target the actual webMAN cache location: `/dev_hdd0/PS2ISO/<filename>.CONFIG`.
- If the same-name HDD cache already exists, PS3 SCALE compares file size, boot serial and PCSX2 ELF hash with the selected USB image. A mismatch blocks all writes and directs the user to remove the stale cache.
- PS2 launch now uses webMAN's documented `/mount_ps3/<path>?emu=ps2_netemu.self` route, then `/play.ps3`, ensuring netemu-only CONFIG command `0x0A` is not silently bypassed by the saved emulator selection.
- New host tests cover HDD and USB CONFIG path resolution, path traversal rejection, and explicit netemu launch routing.
- GitHub Actions run #1022 on commit `da3c88f61eea95aa0f8bcd7c0d5503f23d8f1fab`: **PASS** — Host-side tests, Support Edge Function checks, PS3 ELF/SELF/standard PKG/GNPDRM PKG build and package validation.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/46000292398
- Artifact: [PS3-SCALE-Alpha-0.36-build-with-embedded-theme](https://github.com/Vikik0/PS3-SCALE/actions/runs/46000292398/artifacts/15197672995) — 25,460,724 bytes; SHA-256 `5d34ee8180c48605ef39a866e012b28889b196f7e8cce1d48a8d14e55b153af5`.
- Build outputs: ELF `f69d585f0a0447b2224e913b2d4c61b192973fc8203080cd83840b779b1098dc`; SELF `715b49a8987d7c78e63997253cf4e0afc8bff97be9941486192c4de4e68cd075`; standard PKG `37267dfa645f3a92ed82c914c8457845448d9df044541245b6d2c2ade69eab0b`; GNPDRM PKG `46ff2a59ded91438d5cb4ffea16599db0732ab5dce80157a3c459fd1524cc7c8`.
- Physical-console testing remains **NOT DONE**. The 480p entries are game-code patches only; actual netemu output mode is not verified.

## Alpha 0.35 — exact patch review and apply-time opcode verification

- The review overlay lists the matched game title, patch type, serial, ELF hash, write count, and every EE address with its original and replacement 32-bit word.
- Original words are cached from the selected boot ELF when the exact candidate is probed, then re-read immediately before installation. Any mismatch cancels the write and asks the user to reopen the Enhancer.
- CIRCLE and navigation cancellation remain unchanged; no file is touched until the second CROSS.
- GitHub Actions run #999 on commit `261da5906c68bf3fbf406efe9a669ec7b9388e77`: **PASS** — all host-side tests (including every one of the 36 PS2 patch candidates against executable ELF mappings), Support Edge Function checks, and PS3 ELF/SELF/standard PKG/GNPDRM PKG build and validation passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45977326170
- Artifact: [PS3-SCALE-Alpha-0.35-build-with-embedded-theme](https://github.com/Vikik0/PS3-SCALE/actions/runs/45977326170/artifacts/15187966759) — 25,453,834 bytes; SHA-256 `edc649792b21e8579a52616f977967049594cbdefb87b35d75be11f1312ebf3e`.
- Build outputs: ELF `665864e8f1b601b11bd32a42e2080e3b12b3a7d9fb88352a0e741e1a731155bf`; SELF `f8707e8688a74faa1c85de8a8fec9eee26b0f753d123a09a0fd2e6f0bbf6d08a`; standard PKG `75c6b6cd4a4177f6ce1371a4abf7ac7805c30c7f8e2e6b3e7fc1c57084e36574`; GNPDRM PKG `1c6cae7caf37dfc6d78b2b776edbe287395d90e24bb11f5b7efe456926d0b706`.
- Physical-console testing remains **NOT DONE**; do not mark any FPS, widescreen or 480p entry as hardware-verified.

- Alpha 0.34 baseline package and test results are retained below.

## Alpha 0.34 — PS2 Enhancer and guarded patch application

- The Enhancer displays the exact boot serial and PCSX2 ELF hash read from the selected PS2 ISO before enabling any game-specific patch.
- The source-pinned catalogue contains 11 FPS, 12 widescreen and 13 480p candidates. A candidate is available only when both serial and ELF hash match and every EE address belongs to a file-backed executable ELF segment.
- Experimental EE code patches require two CROSS presses. The first press arms the selected action; the second applies it. CIRCLE, D-pad navigation, leaving the screen or opening the display guide cancels the pending action before any CONFIG write.
- Reapplying the exact same patch is now idempotent: the app reports ALREADY INSTALLED and leaves CONFIG unchanged. Partial overlaps or different original/replacement words at the same address are rejected.
- CONFIG writes retain the verified backup, staged temporary file, atomic install and rollback flow. The original ISO remains read-only.
- The visible app version and package SFO metadata are both `0.34`; a host-side contract test now checks that they cannot drift apart.
- GitHub Actions run #988 on commit `cf850c558550a3e6acdc76bad51578830bb86df8`: **PASS** — Host-side tests, Support Edge Function checks, and PS3 ELF/SELF/standard PKG/GNPDRM PKG build and validation all passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45973073598
- Artifact: [PS3-SCALE-Alpha-0.34-build-with-embedded-theme](https://github.com/Vikik0/PS3-SCALE/actions/runs/45973073598/artifacts/15185034240) — 25,450,588 bytes; SHA-256 `54db642b381387fbe4828529db57237af7008b7683466b55d334b06b7cfa75b3`.
- Build outputs: ELF SHA-256 `1ec375551248bd9b444e29b35c11beec75711cbd78f05231e6e8362f8ad0181c`; SELF `ccdb3bc53fa5ec7912a82344518bff616e33c5afe9892d5ff8a853e5800c530f`; standard PKG `33cf67808fb65a71cfe1fc730a762b13a53947cad88dbf2d820a1c167669e5db`; GNPDRM PKG `f25208e5f7481d99b239cecf4a167c122c419e7061f9e5d1f3d79905843f7936`.
- Physical-console testing is still **NOT DONE**. The reported test target is a PS3 Slim with CFW 4.93 and Cobra 8.5; exact CECH model and CEX/DEX mode have not been confirmed on-device. No FPS, widescreen or 480p candidate is marked verified.

## Alpha 0.33 — durable support outbox and retry queue

- Application/package version: `ALPHA 0.33` / `00.33`.
- Added a portable support JSON builder with strict UUIDv4 validation, 100-character title and 2,000-character details limits, JSON escaping and UTF-8 support.
- Reports are saved atomically as one JSON file per request under the local support outbox.
- Pending reports survive app restarts and can be retried from Help & Support; the queue checks UUID/file-name matching and rejects oversized/corrupt payloads.
- Retry requests reuse the original UUID; the Edge Function performs idempotent inserts and atomic HMAC-based rate limiting (8 reports per UTC day).
- Supabase reports and rate-limit tables have RLS enabled, public/authenticated access revoked, and service-role-only database grants.
- Supabase JS SDK is pinned to `2.119.2`; the Deno lockfile is committed and the Edge Function is type-checked in CI.
- The PS3 app includes only the public publishable-key placeholder; secret keys remain server-side.
- The public Edge Function reads request bodies incrementally, cancels streams above 12 KiB, and rejects invalid UTF-8 before JSON parsing.
- The Help & Support form warns against including PSN IDs, passwords or account tokens; its labels and delivery/error states are localized in Arabic and Spanish.
- GitHub Actions uses per-branch concurrency so newer runs cancel stale duplicate push/PR builds.
- Supabase project creation/deployment is pending creation of a separate Free Plan organization. The connected Supabase account currently exposes only VIKIK (Pro); no project has been created. Never create this backend under VIKIK. The URL and publishable key remain placeholders, so live report submission is intentionally disabled.
- GitHub Actions run #903 on commit `59f2fd42046def62ecb571f0ead1bfc8777a2c6c`: **PASS** — Host-side tests, support Edge Function Deno type-check, Arabic/Spanish locale tests, and PS3 ELF/SELF/standard PKG/GNPDRM PKG build and package validation all passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45801455025
- Artifact: `PS3-SCALE-Alpha-0.33-build-with-embedded-theme` (25,376,710 bytes).
- ELF: SHA-256 `f4fb9118dc440bc3d2c04bb1ce939c5adc17439ce977a47f3b8562ffb691017e`.
- SELF: SHA-256 `7972f418ae19e276e8f95f6462ba4d237dbb77844b16f81e36b484175f462650`.
- Standard PKG: SHA-256 `d3f02a12d8148569afc8de1fab445595a05575a64ee1ea53a623659f35746183`.
- GNPDRM PKG: SHA-256 `99e76a7c70b10fb5f52676e03a47aeff014070e73cd1eb063f4e76c116b71ddf`.
- Artifact ZIP SHA-256: `d3e0d4895411a4a0b0232b47179b831a7bb6b04573ad87c67f864e15b87a03a6`.
- Physical PS3 validation of offline queue recovery and online submission remains pending.

## Alpha 0.32 — atomic persistence and download hardening

- Settings and favorites now use unique exclusive same-directory temporary files, full write loops, fsync and atomic rename. A failed save leaves the previous file intact.
- One-launch webMAN script installation uses O_EXCL; pre-existing temporary files and symlink paths are preserved rather than followed or overwritten.
- The local webMAN HTTP transport now has loopback integration coverage for accepted 2xx launch responses, rejected redirects, split PS3MAPI JSON bodies and bounded no-response timeouts.
- HTTP select/connect retries rebuild their descriptor sets and timeout values after EINTR; launch success requires a 2xx response rather than any 3xx redirect.
- HTTPS downloads use unique PID/time/sequence temporary names with O_EXCL. A stale temporary file does not block later attempts.
- Catalog, community NCL and PS2 CONFIG workers only remove destination files that the current worker successfully downloaded. Network failures no longer delete pre-existing destination files.
- Added host tests for atomic settings/favorites writes, symlink-safe replacement, profile temp-file preservation and download-failure cleanup.
- Updated package version, Arabic guide, root README, online database documentation and CI artifact names to Alpha 0.32.
- GitHub Actions run #759 (final Alpha 0.32 head): **PASS** — all host-side tests and the PS3 ELF/SELF/standard PKG/GNPDRM PKG build and package validation passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45717383555
- Artifact: `PS3-SCALE-Alpha-0.32-build-with-embedded-theme` (25,350,641 bytes).
- ELF: 3,588,222 bytes — SHA-256 `8dded3874cea40e74abffb0df815e2cce78fcb362994aeee97d62ac36817c397`.
- SELF: 788,144 bytes — SHA-256 `dfdfd83444c402698f729877d9fa9b02f614489c65cb9e7c11ac36331480af86`.
- Standard PKG: 11,720,160 bytes — SHA-256 `a39eea4e8a9bc0c1d610fcaf3fa8680da0cf84e653d84f381ed0d59495f223cf`.
- GNPDRM PKG: 11,720,160 bytes — SHA-256 `a57e7f9ba22bb8b82779a72fd2995efb9ebc9518d2b6a1f9be7985923cfafd1a`.
- Artifact ZIP SHA-256: `fbeb685881385298ca357b651698691a8f9b686716db723fc4e214c8993d9dba`.
- Physical PS3 Slim / CFW testing remains pending.
- Online support submission remains local-outbox only until a dedicated PS3 SCALE Supabase project is deployed and configured.

## Alpha 0.31 — PS2 CONFIG Manager, region matching and verified community downloads

- Added the controller-operated PS2 CONFIG Manager, opened from PS2 Game Details with SQUARE.
- Supports standard PS2 ISO `.CONFIG` sidecars and webMAN PS2 Classics `.ENC` files.
- Backup is bounded, verified byte-for-byte and never overwrites an existing backup.
- Restore uses a temporary copy and atomic rename; an existing current CONFIG is retained as a rollback copy.
- Replacing an existing CONFIG requires the saved backup to match the current file byte-for-byte. Stale backups and existing rollback/temp files block replacement.
- The original ISO, EBOOT and installed title files remain read-only. The Triangle generated-CONFIG preview remains memory-only.
- Game metadata infers a conservative region from Title ID / disc serial and displays it in Game Details. FPS profiles require exact Title ID, region and APP_VER matching.
- Community NCL installs require exact game ID and APP_VER, exact file size, and a verified GitHub Git-blob SHA-1 before the downloaded file is moved into Artemis. Legacy cache entries without hashes remain discovery-only.
- The generated community index contains 7,516 records (about 1.86 MB); host tests load the full index and validate every record hash.
- Catalog and community index URLs point to the current development branch. The community index builder refreshes automatically when its script or workflow changes.
- GitHub Actions run #665: **PASS** — all host tests (including the full community index and multi-block Git-blob SHA-1 vectors), ELF/SELF, standard PKG, GNPDRM PKG and package validation passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45353775045
- Artifact: `PS3-SCALE-Alpha-0.31-build-with-embedded-theme` (25,343,184 bytes).
- Cryptographic catalog signatures remain pending; Git-blob SHA-1 is integrity verification, not a signature.
- Physical PS3 Slim / CFW 4.93 testing of manager navigation, backup, restore, verified NCL installation and actual game behavior remains pending.

## Alpha 0.30 — reliable unmodified launch fallback

- When CROSS/START explicitly requests a game launch and safe profile arming fails for recoverable reasons (game identity/version mismatch, PS3MAPI unavailable, or profile validation/write failure), the app now attempts a normal unmodified launch through webMAN.
- The fallback is allowed only while the current Game Details selection still matches the captured Title ID, APP_VER and path. If the selection changed, no game is launched.
- A missing/unresponsive webMAN endpoint still blocks launch with a clear error. Existing user-owned webMAN scripts are never overwritten or removed.
- Before either fallback path starts the game, the app removes only its own leftover script and blocks launch if the script is foreign or cannot be verified. The launch worker repeats the check before handling PS3MAPI loss or a game-version mismatch.
- The one-shot fallback flag is consumed by exactly one launch attempt; the selected game is never accidentally launched with an old profile.
- GitHub Actions run #595: **PASS** — host regression suite, PS3 ELF, CEX SELF, standard PKG, GNPDRM PKG and package integrity checks all passed.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45087625056
- Artifact: `PS3-SCALE-Alpha-0.30-build-with-embedded-theme` (24,693,452 bytes), SHA-256 `e9ef7af6e9f81c45a893e2aed5f6dc653d771280789dd89fac3175ce47363ff5`.
- ELF: 3,504,366 bytes — SHA-256 `2bee0f9396d1a9f7dcf1929eca04d17e441702d9e64c0af689e22065e33d5954`.
- SELF: 779,760 bytes — SHA-256 `92989bf3ca5f2a3ffe2966d2869774cdd46b575b521681d2ffc6f8485561ce47`.
- Standard PKG: 11,402,800 bytes — SHA-256 `a088ed2c827eb645c1d1e4a0127ae29a95413798e2cd96507bb90ecc1e5ed8ce`.
- GNPDRM PKG: 11,402,800 bytes — SHA-256 `097f702b70e51179f812fa57eabfda1dd558de4d10004e5143c217dd3b7830a3`.
- Physical PS3 Slim / CFW 4.93 / Cobra 8.5 launch, controller and game-transition checks remain required; CI cannot validate console runtime behavior.

## Alpha 0.29 — launch freeze and stack-safety hardening

- Moved exact Title ID + APP_VER revalidation for profile arming and profile-backed launches into background workers. HDD and mounted-disc scans no longer run in the controller input loop.
- Replaced the 256-entry local scan lists (about 300 KiB each) with one serialized static validation buffer, avoiding PPU stack exhaustion on the main thread and 0x8000-byte workers.
- If the game identity or update version changes between arming and launch, PS3 SCALE removes only its own pending profile script and refuses the enhanced launch.
- Added bounded file-size checks before decoding local cover and before/after PNGs; missing, non-regular or oversized images are skipped.
- Added a dedicated host regression test for worker-only validation, static scan storage and bounded preview loading.
- GitHub Actions run #573: **PASS**. Host regression tests, PS3 ELF link, CEX SELF generation, standard PKG generation, GNPDRM PKG generation and package integrity checks all passed. Physical-console verification remains required.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45086065984
- Build artifact: `PS3-SCALE-Alpha-0.29-build-with-embedded-theme` (24,692,403 bytes).
- ELF: 3,504,263 bytes — SHA-256 `1cfa4cd1d468b10e4fb5fdf7b7ca6790b27a09c8e33f6e2d1b404659d26772d8`
- SELF: 779,456 bytes — SHA-256 `087137f815e3502cba7be1fded27177b565b137c447894f57dfb72331ffba129`
- Standard PKG: 11,402,544 bytes — SHA-256 `42eb0d4ae13787d317dde21cd1672409c89c1aa14f628fdec215a455b9ae933b`
- GNPDRM PKG: 11,402,544 bytes — SHA-256 `3915cffd99d6dcf4b4a0fea061df226d206c5cddfaa8e28f9625a68acda72466`
- ZIP SHA-256: `9ba743c22176cb6377cbae1f75c9cc1fb9db0cd4c82a972177827f583d0ade5a`
- Target console: PS3 Slim / CFW 4.93 / Cobra 8.5 (user-reported); actual game launch and controller checks still need to be performed on the console.

## Alpha 0.28 — PS2 ISO format compatibility

- Extended PS2ISO scanning to recognize `.ISO`, `.BIN`, `.IMG` and `.MDF` case-insensitively.
- Extended the webMAN launch-route validator to accept the same four standard PS2 ISO extensions, so a listed IMG/MDF title can be launched through the normal path.
- Kept `.BIN.ENC` classified separately as a PS2 Classic package; it is not treated as a standard ISO.
- Added host scanner fixtures that verify IMG/MDF entries and recover their serials from filenames, plus route tests for encoded IMG/MDF paths.
- Alpha 0.28 GitHub Actions run #562: **PASS**. Host tests, PS3 ELF/SELF build, standard PKG and GNPDRM PKG generation and package checks all passed. Physical-console scan / launch checks remain pending.

## Alpha 0.27 — Companion launch reliability

- Removed the hard block that prevented Apollo / Artemis launch when the webMAN plugin file was not found at the two common paths. Those paths are now treated as hints only; the bounded local HTTP request is the authoritative runtime check.
- The Companion Tools UI now labels the path check as `FILE HINT`, and tells the user when it is trying the local service despite a missing hint.
- Added shutdown joining for the companion launch worker so the app cannot tear down shared state while a local webMAN request is still running.
- Added static regression checks for the fallback path and shutdown join.
- Alpha 0.27 GitHub Actions run #561: **PASS**. Host tests and PS3 ELF/SELF/PKG builds passed. Physical-console Apollo / Artemis launch checks remain pending.

## Alpha 0.26 — Companion Tools launch integration

- Added a controller-selectable Apollo / Artemis launcher to Companion Tools.
- Apollo and both supported Artemis package IDs are routed through the validated webMAN /play.ps3?<TITLE_ID> path.
- The request runs on a worker thread with bounded local HTTP timeouts; failed requests leave PS3 SCALE open.
- Added host tests for valid/invalid app IDs and static UI contract checks for navigation, launch and refresh actions.
- Apollo save operations and Artemis NCL execution remain in their own applications; this change does not claim in-app reimplementation.
- Alpha 0.26 CI run #559: **PASS**. Host tests and PS3 ELF/SELF/standard PKG/GNPDRM PKG builds all passed.
- Physical-console launch checks for Apollo and both Artemis package IDs remain pending.

## Latest code/package CI validation

GitHub Actions run **#562** completed successfully on 2026-11-21.

- Host-side regression tests, including the new PS2 IMG/MDF scanner and webMAN route tests: **PASS**
- Arabic bitmap preview render: **PASS**
- Staircase material/motion preview render: **PASS**
- PS3 ELF link: **PASS**
- CEX SELF generation: **PASS**
- Standard PKG generation: **PASS**
- GNPDRM PKG generation and package checks: **PASS**
- Embedded XMB artwork byte-for-byte verification (`ICON0.PNG`, `PIC0.PNG`, `PIC1.PNG`): **PASS**
- Embedded Apollo Save logo, Music 1 source and Music 2 theme: **PASS**

Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/44963475576

## Build artifact

Artifact name: `PS3-SCALE-Alpha-0.28-build-with-embedded-theme`

Contains:
- `ps3scale.elf` — 3,504,173 bytes
- `ps3scale.self` — 779,280 bytes
- `ps3scale.pkg` — 11,402,352 bytes
- `ps3scale.gnpdrm.pkg` — 11,402,352 bytes

SHA-256:
- ELF: `ea70b72be8e36171a526bf6331a38a920b742b9a0942f44e5ec18a605a40a128`
- SELF: `bb4c38d211b6800554f7855803dcbfa31bc1a7ecf1e0d8d178f506e7b0527353`
- Standard PKG: `18a35d68a448afc4cf656e8c87537d21aae168ce8c108dede39db4d9857bf34e`
- GNPDRM PKG: `66b6c976afd36e833952f1974b6c2909acdc0d14a7cbabb80e9481b7d5a76cac`
- Build ZIP: `a8cf24472c8adbf99f5546f911f957360f506fac7361d661535565f980e6a1eb`

## Alpha 0.23 changes in this continuation

- Removed the FEATURES heading from Home and removed the duplicate image panel from About. The XMB `ICON0.PNG`, `PIC0.PNG` and `PIC1.PNG` use the approved pearl/lavender staircase identity.
- Renamed the visible playlist entries to Music 1 and Music 2.
- Added the Enhancement Storage setting: Temporary by default or Permanent for new Artemis NCL files.
- Online DB now refreshes automatically on first open, uses Triangle for manual refresh and Select for search, and defaults to Graphics + Any Region/Version.
- Added strict Title ID + APP_VER matching for community installs; a same-title/same-region candidate is browse-only and cannot authorize a download.
- Added exact-match `GRAPHICS_FPS` NCL installation to the detected Artemis USERLIST directory. Both upstream `ARTPS3001` and Artemis R5 `ARTZ00001` package IDs are supported. The app validates the source path, exact Title ID + APP_VER, text format and size, and never overwrites an existing file.
- Added an exact byte-size check against the refreshed community index. If the upstream NCL changes after indexing, the download is discarded and the user is asked to refresh the Online DB.
- Temporary NCL files are tracked and removed on the next PS3 SCALE start. Permanent files remain until manually removed. PS3 SCALE does not execute or auto-apply NCL files; the user selects them in Artemis.
- Replaced the synthetic Game Details comparison illustration with an exact-version local before/after PNG loader. No genuine game screenshot pairs are bundled yet; the UI reports a missing pair instead of showing a concept image.
- Fixed webMAN local HTTP initialization by loading NET/NETCTL and calling `netCtlInit()`. Added distinct launch error messages.
- Fixed the PS3MAPI availability probe to read and validate the complete JSON response body rather than relying on the headers-only packet. This avoids false "MAPI offline" results when HTTP headers and body arrive separately.
- If PS3MAPI is unavailable after a PS3 SCALE profile was armed, the app removes its own one-launch script and still attempts an unmodified game launch.
- Updated package SFO metadata to 00.23 and aligned GitHub Actions artifact names.

## Alpha 0.24 changes in this continuation

- Replaced the generated placeholder XMB icon with the approved square PS3 SCALE Neon Glass image, centered without stretching in the 320×176 ICON0 canvas.
- Removed the extra decorative rings around the in-app spiral staircase; the Home scene now renders one staircase only.
- Online DB defaults to All Files + Any Region/Version and lists unmatched community entries as browse-only immediately on first open. Triangle still forces a live refresh.
- CROSS/START in Game Details now arms an exact one-write B_COMMUNITY memory profile and launches the game. No exact profile means the original game starts unmodified; manual and multi-write profiles remain blocked.
- Kept the real Before/After screenshot loader: no synthetic green preview is shown. Genuine captures are still required per Title ID + APP_VER.

## Alpha 0.25 XMB correction

- Alpha 0.24 updated `ICON0.PNG`, but the large selected-item artwork could still show the older staircase-and-text `PIC0.PNG`, making the main image look unchanged on the console.
- Alpha 0.25 now generates both `ICON0.PNG` and `PIC0.PNG` from the same approved Neon Glass square logo. The old generated staircase/text panel is removed from `PIC0.PNG`.
- `PIC1.PNG` remains the separate full-screen pearl/lavender wallpaper with one staircase.
- Package verification compares every embedded XMB PNG byte-for-byte against the freshly generated file, as well as checking PNG dimensions.
- Package version is bumped to `00.25` to ensure the PS3 receives a distinct update build.

## Remaining verification

- **No physical PS3 installation or game launch was performed by CI.** Install the GNPDRM PKG on the target CFW console and test controller navigation, scan, Online DB refresh, Artemis USERLIST loading, temporary/permanent cleanup, webMAN launch, and one safe memory profile.
- Real before/after screenshots still need to be captured and supplied for each supported Title ID + APP_VER. The loader is implemented, but screenshot coverage is not complete.
- Community NCL files and memory profiles remain community-supplied. Exact matching, index-size validation and text checks do not prove that a patch is safe or hardware-tested.
- Remaining compiler/configure warnings are from bundled third-party mpg123 / PS3 network dependency sources (for example, unused parameters, signedness and optional libpsl/IDN/manual support). The PS3 application source builds successfully.

Do not merge Draft PR #20 until the physical-console smoke test and review are complete.