# Optional webMAN MOD / PS3MAPI integration

## Purpose

PS3 SCALE may use webMAN MOD's PS3MAPI as an optional bridge for title-specific, in-memory PS3 enhancements. This is a candidate backend, not a requirement for the game scanner and not a universal upscaler.

PS3MAPI exposes process enumeration, process/module information, and memory read/write operations. webMAN MOD can expose commands over HTTP through its `/ps3mapi.ps3?<command>` endpoint; the native PS3MAPI service uses port 7887. The server must be enabled in webMAN MOD settings.

## Proposed integration stages

### Stage 1 — read-only connection check
- Detect whether the local webMAN MOD endpoint responds.
- Query firmware version and current process ID/name.
- Display connection status and the running Title ID if it can be resolved.
- Do not read console identifiers (IDPS/PSID), alter syscalls, change clocks, or write memory.

### Stage 2 — per-title patch discovery
- Resolve the exact Title ID, region and game update version.
- Load only a catalog record matching that exact identity.
- Show patch author, source, hash, confidence tier, known issues and expected effect.
- Require the user to explicitly confirm a patch before any memory write.

### Stage 3 — guarded memory patching
- Read and verify the original bytes at each target address.
- Write only the replacement bytes listed in a signed/verified manifest.
- Re-read the address to verify the change.
- Restore original bytes on request and on a detected mismatch where safe.
- Refuse the patch if the process, title ID, version, expected bytes or memory region differs.

## Security rules

- webMAN MOD is optional; PS3 SCALE must still work without it.
- Keep the MAPI service restricted to the local trusted network. Do not expose it to the public internet.
- Start with read-only queries. Memory writes can crash a game or the console and are not enabled in Alpha 0.2.
- Never use PS3 SCALE to fetch, display, store, or modify IDPS/PSID.
- Never disable syscalls, spoof console identity, change fan settings, or change CPU/RSX clocks as part of a graphics profile.
- A community patch is not hardware-verified merely because it works in RPCS3 or on another console.
- Some workflows/tools may require a debug EBOOT for game modification; the exact requirement must be validated for each CEX title and method before implementation.

## Test requirements

1. Confirm the user's webMAN MOD version and that PS3MAPI is enabled.
2. Test read-only connection on CECH-2504A / CEX CFW 4.93 / Cobra 8.5.
3. Verify the current process/title identity while at XMB and while a game is running.
4. Confirm that a read-only request causes no change to game state.
5. Only then test a harmless, reversible, title-specific memory patch with known original bytes.
6. Record before/after behavior and a rollback result.

## References

- webMAN MOD Web Commands / PS3MAPI command list: https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- webMAN MOD project and features: https://github.com/aldostools/webMAN-MOD
- PS3MAPI overview and CEX/DEX support: https://consolemods.org/wiki/PS3%3AMAPI
