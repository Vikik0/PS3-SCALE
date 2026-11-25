# Phase 103 — host model for Campaign session-monitor recovery

## Change

Added a portable C state-machine model for the long-lived GSC session monitor. The model separates:
- candidate native-pair validation (split-screen active, fresh player array, exactly two distinct valid player entities, one primary and one secondary);
- current bound-pair validation (both bound handles still present and Campaign aliases still match);
- eight consecutive failed validations before invalidation;
- active-lock release while completed-event history is preserved;
- waiting and rebind after reconnect;
- idempotent start and explicit stop.

Three Python source-contract tests tie the model's lifecycle rules to the actual GSC watcher, and CI compiles/runs the C model independently.

## Verification

- Python suite: **542/542 passed**.
- Event Lock C model: **21/21 passed**.
- GSC wrapper parser: **10/10 passed**.
- Mocked GSC loader: **12/12 passed**.
- Session-monitor C model: **14/14 passed**.
- GSC Tool compile/decompile, PS3 CEX PRX, both read-only controller probes and importer link validation passed.
- CI run [#1588](https://github.com/Vikik0/PS3-SCALE/actions/runs/45723183499): **7/7 jobs successful**.
- Capture kit: **57/57 files**, **537,726 bytes**, SHA-256 `51a0b7a87a81db67fb476206001915adb2ea6d93092a2dcc4dbb9c2f6cfdc47b`; artifact ID `15072319880`.

## Boundary

The host model proves the defined state transitions, not the existence of a second native Local Client or the behavior of the retail game. Physical PS3 reconnect remains unverified.
