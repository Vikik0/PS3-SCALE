# PS3 SCALE patch trust policy

No patch is installed automatically in Alpha 0.2.

## Trust tiers

- **Tier A — Hardware verified:** tested on real PS3 hardware for the exact Title ID and update version, with tester/source evidence.
- **Tier B — Community reported:** reported working on real hardware, but not independently reproduced by PS3 SCALE.
- **Tier C — Emulator converted / unverified:** imported from RPCS3 or other sources without a real-hardware confirmation.
- **Blocked:** known to freeze, corrupt, mismatch the game version, or have an unresolved safety issue.

## Required before apply

1. Exact platform (PS3/PS2).
2. Exact Title ID or PS2 serial.
3. Exact game update version / region where applicable.
4. Patch type and target file or memory range.
5. SHA-256 and expected byte size.
6. Source and author/license metadata.
7. Backup and rollback procedure.
8. Known issues and confidence tier.

## Hard rules

- Never install Tier C automatically.
- Never install a patch with a Title ID or version mismatch.
- Never patch the original game folder without an explicit user confirmation and a verified backup.
- Do not download executable files and run them.
- Keep a local denylist for known crashes.
- A patch described as FPS unlock is not frame generation and does not guarantee a stable target FPS.
