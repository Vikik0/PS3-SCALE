# Personal game-inclusive PS3 build

The public repository intentionally does not contain the Crash Bash disc image or a BIOS. Keep the original BIN/CUE archive in the user's private Dropbox storage.

## Current verified runtime

The latest successful PS3 runtime build is GitHub Actions run `45724536491` on branch `crash-bash-enhanced`. It produced an unexpired `Crash-Bash-RetroArch-Stage` artifact and a successful unified lobby/runtime PKG. The personal-build workflow validates the selected run and artifact before downloading it.

## One-time private archive handoff

1. Keep the ZIP at the private Dropbox path `/Crash-Bash-Temporary-Transfer/Crash-Bash-Original-Disc-Temporary-Transfer.zip`.
2. Generate a single-use Dropbox temporary download URL immediately before the personal build. Use a 900-second lifetime. Do not open, preview, HEAD-check, or otherwise request the URL; its first HTTP request consumes it.
3. In GitHub, open **Settings → Secrets and variables → Actions → New repository secret**.
4. Create the secret named `CRASH_BASH_DISC_ARCHIVE_URL` and paste the temporary Dropbox download URL as its value.
5. Open **Actions → Crash Bash Personal Game-Inclusive PS3 Build → Run workflow** on the `main` branch. Keep the default `runtime_run_id` value `45724536491`.
6. The workflow downloads the archive once, validates the exact BIN/CUE members and hashes, embeds the game, builds one PKG, and encrypts the output for private transfer.
7. Download the `Crash-Bash-Online-Personal-PS3-Encrypted` artifact before its one-day retention expires.

If the temporary URL expires or is consumed before the workflow downloads it, generate a fresh URL and update the secret before starting a new run. The workflow never prints the URL and masks it in its logs.

## Private decryption

The matching private key was generated and delivered separately as `Crash-Bash-Personal-Build-Key-Package.zip`. Keep `Crash-Bash-Personal-Build-Private-Key.key` offline and private; it is not stored in GitHub or Dropbox. The public certificate is committed at `keys/personal-build-public.pem`.

Certificate SHA-256 fingerprint:

```text
9ba2aaf245b0f19d5d0e9ee023d4d9c96291437b7a26e4c7d9d1cf65c2770bcd
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
- No BIOS is included. Use HLE on every console or supply the same BIOS dumped from the user's own console.
- A successful cross-build does not certify actual gameplay. Follow `HARDWARE_TEST_PLAN.md` for local, two-console LAN, internet relay and four-player checks.
- Cross-play with `crashbash.app` remains blocked until its owner grants written permission and supplies a supported client/API contract. Do not reverse-engineer or automate its private client.
