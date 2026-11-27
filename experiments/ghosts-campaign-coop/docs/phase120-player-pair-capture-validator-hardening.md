# Phase 120 — runtime analyzer package closure and player-pair validator hardening

## Purpose

The capture kit contained the combined Campaign/Multiplayer analyzer but omitted several modules it imports. On a clean extracted kit, the analyzer could fail before reaching the actual dump checks. This phase closes that dependency gap and hardens the native player-pair evidence validator.

## Changes

- Package the native Campaign player-pair JSON template, structural validator, and Campaign SELF hash-binding helper.
- Include the complete Python import closure used by the combined analyzer, including the PPU/Ghidra helper modules.
- Include the three optional Ghidra Java scripts so the optional headless PPU inventory path is available from the kit.
- Make the kit verifier reject a package missing any required analyzer dependency.
- Preserve the longest valid post-loss reconnect streak. A later malformed sample, identity change, or continuity break no longer erases an already observed qualifying reconnect run.
- Reject missing required observation fields clearly and treat non-object player records as rejected samples instead of allowing a TypeError to escape.

## Validate the package

From the project root, run the complete test suite:

~~~bash
python -m unittest discover -s tests -v
~~~

The extracted capture kit can be checked with:

~~~bash
python Tools/verify_capture_kit.py
~~~

The native player-pair template is:

~~~text
Tools/ps3mapi/CAMPAIGN_NATIVE_PLAYER_CAPTURE_TEMPLATE.json
~~~

Fill it only with measured runtime values. Do not invent entity IDs, memory addresses, camera/HUD owners, or sample timestamps. Calculate the SHA-256 of the exact Campaign default.self and put it in campaign_self_sha256. Then run the structural validator:

~~~bash
python Tools/ps3mapi/validate_campaign_player_pair_capture.py --capture campaign_player_pair_capture.json
~~~

The combined analyzer checks the declared Campaign SELF hash against the exact SELF file it analyzes when the pair capture is supplied. A missing hash remains explicitly unbound. Structural validation does not authenticate the operator or establish that the observations came from a physical PS3.

## Runtime boundary

This phase fixes packaging and evidence-validation behavior only. It does not create a native Local Client, spawn a second Campaign player, prove independent camera/HUD execution, or migrate any original mission handler. No retail EBOOT, SELF, or FastFile was modified. Player 2 remains NOT_PROVEN and the four shared mission interactions remain 0/4 ready.
