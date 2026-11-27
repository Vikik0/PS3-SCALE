# PS2 Enhancer — Physical Hardware Test Report

**Current status:** Not yet tested on a physical PS3. Every FPS, widescreen and 480p entry below remains experimental.

This report is the acceptance record for the PS2 Enhancer. A successful host test, ELF/SELF/PKG build, CONFIG write or exact game match is not proof of a working enhancement.

## Planned target device (reported project setup; not yet physically tested)

- Device family: PS3 Slim.
- System software: CFW 4.93.
- Cobra: 8.5.
- Exact CECH retail model and CEX/DEX mode remain unconfirmed on-device.
- This is the intended test target only. No physical test has been performed; the actual test record below remains unfilled.

## Actual test environment (pending physical run)

| Field | Recorded value |
|---|---|
| PS3 model / CECH code | NOT RECORDED |
| CFW / HEN and version | NOT RECORDED |
| PS3 system software | NOT RECORDED |
| PS2 Classics / netemu version | NOT RECORDED |
| Video output mode | NOT RECORDED |
| Display refresh rate | NOT RECORDED |
| Controller model | NOT RECORDED |
| Test date / tester | NOT RECORDED |
| PS3 SCALE build commit | `5147da208a6b7b29865f5a8cb1115febb513b608` |

## Required procedure for each title

1. Confirm the disc region, game serial and exact boot ELF hash shown by PS3 SCALE.
2. Record the unmodified CONFIG state and verify the original ISO size and SHA-256.
3. Run the game without the candidate patch. Record the same repeatable gameplay scene, frame rate, audio, FMV and controller behavior.
4. Apply only the exact matching candidate. Confirm the original CONFIG backup is byte-identical and the ISO is unchanged.
5. Repeat the same scene and record measured results. Do not estimate FPS from visual smoothness alone.
6. Test at least one long gameplay session, one FMV / cutscene and all relevant controller inputs.
7. Check for speed-up / slow-down, collision, animation, physics, audio desynchronization, hangs, crashes and save issues.
8. Restore the original CONFIG and confirm the game returns to baseline behavior.
9. Mark a candidate VERIFIED only after repeatable physical-console results are recorded.

## 60 FPS candidate register

All source files are pinned to PCSX2 patch commit `1fd98102b8e2fffe3e0c29aab55339d48af0ffe5`. Only the single direct word replacement from each `[60 FPS]` section is imported.

| Title | Serial | ELF hash | EE address → replacement | Status |
|---|---|---|---|---|
| Ed, Edd n Eddy: The Mis-Edventures | `SLUS_212.60` | `B0AE2D8A` | `00161860 → 2C620000` | NOT TESTED |
| Scooby-Doo! Night of 100 Frights | `SLUS_203.49` | `10B1F370` | `00131DB0 → 2C420001` | NOT TESTED |
| Teen Titans | `SLUS_211.83` | `E1F17139` | `0015FF94 → 2C620000` | NOT TESTED |
| Disney's Kim Possible - What's the Switch | `SLUS_214.37` | `86B490D1` | `0015C844 → 2C620000` | NOT TESTED |
| Happy Feet | `SLUS_214.55` | `BAFEFC57` | `0015CDD4 → 2C620000` | NOT TESTED |
| The Ant Bully | `SLUS_214.15` | `E10A865B` | `0015CCD4 → 2C620000` | NOT TESTED |
| Indiana Jones and the Staff of Kings | `SLUS_218.85` | `B15F0BD6` | `001505B4 → 2C620000` | NOT TESTED |
| Jimmy Neutron: Jet Fusion | `SLUS_206.96` | `FCC09959` | `0022CA0C → 28420001` | NOT TESTED |
| Shin Megami Tensei - Devil Summoner 1 | `SLPM_662.46` | `67D76160` | `0011B560 → 10000006` | NOT TESTED |
| Manhunt | `SLUS_208.27` | `38DEA143` | `00548960 → 08000001` | NOT TESTED |
| Pirates of the Caribbean: The Legend of Jack Sparrow | `SLUS_211.10` | `632E3976` | `00384CD0 → 10000004` | NOT TESTED |

## 16:9 widescreen candidates

| Title | Serial | ELF hash | Status |
|---|---|---|---|
| Scooby-Doo! Night of 100 Frights | `SLUS_203.49` | `10B1F370` | NOT TESTED |
| Teen Titans | `SLUS_211.83` | `E1F17139` | NOT TESTED |
| Disney's Kim Possible - What's the Switch | `SLUS_214.37` | `86B490D1` | NOT TESTED |
| Jimmy Neutron: Jet Fusion | `SLUS_206.96` | `FCC09959` | NOT TESTED |
| 7 Sins (PAL-M) | \`SLES_532.80\` | \`52DEB87B\` | NOT TESTED |
| Shadow Hearts (NTSC-U) | \`SLUS_203.47\` | \`8BE3D7B2\` | NOT TESTED |
| Teenage Mutant Ninja Turtles 2: Battle Nexus (PAL-M5) | \`SLES_530.17\` | \`C7375C85\` | NOT TESTED |
| Obscure (NTSC-U) | \`SLUS_207.77\` | \`A06047D6\` | NOT TESTED |
| Armored Core 3 (PAL-E) | \`SLES_513.99\` | \`DE5AF9A8\` | NOT TESTED |
| Bionicle (NTSC-U) | \`SLUS_208.18\` | \`8371B27B\` | NOT TESTED |
| Tom Clancy's Splinter Cell: Double Agent (PAL-M3) | \`SLES_538.27\` | \`6BD0E9C2\` | NOT TESTED |
| La Pucelle: Tactics (PAL) | \`SLES_529.78\` | \`474EC389\` | NOT TESTED |

## 480p progressive-output candidates

| Title | Serial | ELF hash | Status |
|---|---|---|---|
| Crash of the Titans (PAL-R) | `SLES_548.43` | `AEFD7D25` | NOT TESTED |
| George of the Jungle (PAL-M) | `SLES_549.75` | `278B7BEA` | NOT TESTED |
| Syberia (PAL) | `SLES_513.93` | `D1ACD489` | NOT TESTED |
| Crash of the Titans (PAL-M) | \`SLES_548.41\` | \`ECA6BFC5\` | NOT TESTED |
| Medal of Honor: European Assault (PAL-M) | \`SLES_533.32\` | \`BB70FFB9\` | NOT TESTED |
| SpongeBob SquarePants: Battle for Bikini Bottom (PAL-F) | \`SLES_536.23\` | \`9ADA98B4\` | NOT TESTED |
| Star Wars: Episode III - Revenge of the Sith (PAL-M3) | \`SLES_531.55\` | \`99AD19EE\` | NOT TESTED |
| Commandos: Strike Force (PAL-M) | \`SLES_527.68\` | \`5E1161DF\` | NOT TESTED |
| The Punisher (PAL-S-I) | \`SLES_530.49\` | \`5AC5D875\` | NOT TESTED |
| Driver: Parallel Lines (PAL-M) | \`SLES_540.27\` | \`D720770D\` | NOT TESTED |
| kill.switch (PAL-M) | \`SCES_521.24\` | \`91A65EAE\` | NOT TESTED |
| Space Chimps (PAL-M) | \`SLES_553.40\` | \`C423A2DC\` | NOT TESTED |
| Cold Fear (PAL-M) | \`SLES_531.58\` | \`E38A0AB6\` | NOT TESTED |

## Per-title result sheet

Copy this section for each candidate tested.

| Measurement | Baseline | Patched | Pass / fail / notes |
|---|---|---|---|
| Measured FPS (same scene) | NOT RECORDED | NOT RECORDED | NOT TESTED |
| TV-reported output mode / resolution | NOT RECORDED | NOT RECORDED | NOT TESTED |
| XMB PS / PS2 Upscaler setting | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Gameplay speed / timing | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Audio sync | NOT RECORDED | NOT RECORDED | NOT TESTED |
| FMV / cutscene playback | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Controller / pressure inputs | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Collision / physics / animation | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Long-session stability | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Save / load behavior | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Backup restore verified | NOT RECORDED | NOT RECORDED | NOT TESTED |
| Original ISO unchanged | NOT RECORDED | NOT RECORDED | NOT TESTED |

**Acceptance rule:** keep the status as NOT TESTED unless the same title and exact serial/hash pair pass repeatable physical-console checks. If any regression appears, restore the backup and mark the candidate BLOCKED; do not compensate by guessing offsets or adding unverified patches.
