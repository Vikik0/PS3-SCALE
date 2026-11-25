# Phase 94 — preserve the original player array during startup rebind

## Change

- Removed the destructive `level.playerpiggyback = undefined` assignment from `main()`.
- Kept the engine-owned player array available while the bridge retries native-player discovery.
- The final fallback bind now requires a fresh `getentarray("player", "classname")` result. If the fresh query is undefined, the bridge sets session status 2 and returns without binding against an older snapshot.
- This complements Phase 93: invalidation rebuilds the original array, and startup/rebind no longer immediately clears it again.

## Verification

- Python suite: **530/530 passed**.
- GSC Tool compile/decompile and PS3 CEX bootstrap passed.
- CI run [#1567](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719086601): **7/7 jobs successful**.
- Bridge source SHA-256: `8a2b05298a5925d398e1ae7e4752a482bcde4196a126311b0cec314788c6e1a6`.
- GSCBIN SHA-256: `9fc7dcc4dde75e159090c55fa7560c26f5c61e890db55c879ed474ead51cc724`; size **3,803 bytes**.
- Capture kit: **57/57 files**, **535,722 bytes**, SHA-256 `6501bd452029220d01e907c711d6128d51e7831643670538b551e78131da5288`; artifact ID `15072135172`.

## Boundary

This is source/CI validation only. It does not establish a second native Local Client or prove Campaign runtime behavior on the PS3.
