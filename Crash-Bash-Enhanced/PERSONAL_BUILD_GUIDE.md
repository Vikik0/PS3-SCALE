# Personal game-inclusive PS3 build

The public repository intentionally does not contain the Crash Bash disc image or a BIOS. Keep the original BIN/CUE archive in the user's private Dropbox storage.

## Current verified runtime

The personal-build workflow automatically selects the newest successful `Crash Bash Enhanced PS3 Build` run on `crash-bash-enhanced` that still has a valid, unexpired `Crash-Bash-RetroArch-Stage` artifact. Leave `runtime_run_id` blank to use this automatic selection, or enter a run ID to pin a specific verified build.

## One-time private archive handoff

1. Keep the ZIP at the private Dropbox path `/Crash-Bash-Temporary-Transfer/Crash-Bash-Original-Disc-Temporary-Transfer.zip`.
2. In GitHub, open **Settings → Secrets and variables → Actions → New repository secret**. GitHub requires a non-empty value; do not create this secret blank.
3. Immediately before starting the build, generate one single-use Dropbox temporary download URL with a 900-second lifetime and set it as the value of `CRASH_BASH_DISC_ARCHIVE_URL`. Do not open, preview, HEAD-check, or otherwise request the URL; its first HTTP request consumes it. Do not paste the URL into chat or a public file.
4. Open **Actions → Crash Bash Personal Game-Inclusive PS3 Build → Run workflow** on the `main` branch. Leave `runtime_run_id` blank so the workflow selects the latest verified runtime automatically.
5. The workflow downloads the archive once, validates the exact BIN/CUE members and hashes, audits the PS1 filesystem, embeds the game, builds one PKG, and encrypts the output for private transfer.
6. Download the `Crash-Bash-Online-Personal-PS3-Encrypted` artifact before its one-day retention expires.
7. After the run completes, delete the `CRASH_BASH_DISC_ARCHIVE_URL` repository secret. The URL is single-use and expires, but removing the stored value avoids retaining an obsolete bearer credential.

If the temporary URL expires or is consumed before the workflow downloads it, generate a fresh URL and update the secret before starting a new run. The workflow never prints the URL and masks it in its logs.

## Original disc archive verification

The private Dropbox ZIP was inspected before dispatch:
- ZIP size: 80,246,798 bytes; archive integrity test passed.
- Exact members: `Crash Bash (USA).bin` (178,220,448 bytes) and `Crash Bash (USA).cue` (82 bytes).
- BIN SHA-256: `a1d6b967a2311f1444482f4af91dff2d75e083ed538419f25d57d77dd5a4d7fd`.
- CUE SHA-256: `de7f3b62c2703f3a1ee76a5d10e5fb27cc386dea26b196807e5eec5e04a8fce9`.
- The CUE references the matching BIN and uses 2352-byte sectors.
- The BIN contains a valid Mode 2 / 2352-byte ISO9660 data track; the primary volume descriptor and root directory parse correctly.
- `SYSTEM.CNF` identifies the boot executable as `SCUS_945.70;1`, and the referenced executable begins with the genuine `PS-X EXE` signature (432,128 bytes, load address `0x80010000`).
- `CRASHBSH/CRASHBSH.DAT` was read from the ISO9660 filesystem (73,220,096 bytes; SHA-256 `3a02b477ab48ae99010e1a9db3b0d591c79e800b43d10779945ca18ab788acb7`); all 992 DAT index entries were checked for overlap and out-of-range offsets, then reconstructed byte-for-byte with the same SHA-256.
- The boot executable SHA-256 is `fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516`.
- The lobby's 64-bit CUE/BIN fingerprint for this exact dump is `49472FA318780E01`; the same value was reproduced from the original Dropbox files and the staged package copy.
- The staging script passed all eight security/regression cases and staged this exact archive successfully.

The workflow repeats ZIP integrity, exact-member, file-size, BIN/CUE SHA-256 and CUE-to-BIN linkage checks inside GitHub Actions before it downloads the runtime or starts the expensive PS3 build. It then parses the ISO9660 filesystem, confirms the `SCUS_945.70` boot executable and verifies the 992-entry DAT table with byte-identical reconstruction. The checks are pinned to the verified dump hashes above. This preflight does not replace the final PS3 PKG build or physical-console testing.

## Private decryption

The matching private key is provided separately to the project owner as `Crash-Bash-Personal-Build-Key-Package.zip`. Keep `Crash-Bash-Personal-Build-Private-Key.key` offline and private; it is not stored in GitHub or Dropbox. The public certificate is committed at `keys/personal-build-public.pem`.

Certificate SHA-256 fingerprint:

```text
d31cc3af70d4a049ba395f9bcd6dce6cd7f6a0e1120e13a7b5fc905168ae14f0
```

After downloading the encrypted artifact, decrypt it locally with the matching key:

```bash
openssl cms -decrypt -binary -inform DER \
  -in Crash-Bash-Online-Personal-PS3.pkg.cms \
  -recip personal-build-public.pem \
  -inkey Crash-Bash-Personal-Build-Private-Key.key \
  -out Crash-Bash-Online-Personal-PS3.pkg
```

Verify the decrypted PKG SHA-256 against the `PKG SHA-256` line in `PERSONAL-BUILD-MANIFEST.txt`. The private key must never be committed, uploaded as a GitHub Secret, or shared with other people.

## Package and hardware limits

The build uses the RGUI menu and keeps the embedded runtime below the PSL1GHT 1,024-entry PKG cap. Before finalization, the package file table is listed and compared against every staged file and directory; missing entries or truncation fail the build.

- One installable PKG contains the native lobby, RetroArch, PCSX-ReARMed and the user's supplied Crash Bash BIN/CUE.
- The original BIN/CUE are copied byte-for-byte into `USRDIR/PSXISO/` and checked with `cmp`; the project adds only the PS3 launcher, lobby and emulator runtime around the retail game. It does not replace Crash Bash with a new game or rewrite the disc image.
- Local play passes the embedded CUE directly to RetroArch; it does not use `--menu`, which would open the frontend without loading Crash Bash.
- No BIOS is included. Use HLE on every console or supply the same BIOS dumped from the user's own console.
- A successful cross-build does not certify actual gameplay. Follow `HARDWARE_TEST_PLAN.md` for local, two-console LAN, internet relay and four-player checks.
- Cross-play with `crashbash.app` remains blocked until its owner grants written permission and supplies a supported client/API contract. Do not reverse-engineer or automate its private client.
