# Crash Bash Enhanced — PS3 homebrew foundation

This directory contains the first native PS3 client foundation for the Crash Bash Enhanced project.

## Current build scope

- Native PSL1GHT / Tiny3D PS3 application.
- XMB-installable homebrew PKG build pipeline.
- Guest profile placeholder that does not use PSN.
- Controller navigation and an offline input test arena.
- Network module initialization diagnostic.
- Host-side protocol serialization test.

## Important status

This is **not yet the complete Crash Bash game** and does not claim online matchmaking is operational. The online rooms screen deliberately reports that the cloud backend is not configured rather than showing a fake successful connection.

No Crash Bandicoot / Crash Bash game assets are included. The original game requires legally obtained assets or a clean-room recreation; this repository does not distribute a game dump, BIOS, copyrighted art, audio, or level data.

## Build

The GitHub Actions workflow uses the existing PS3 toolchain container used by the parent repository:

- ghcr.io/02900/ps3-toolchain-tiny3d:latest
- make pkg

Expected outputs: crashbash.elf, crashbash.self, crashbash.pkg, crashbash.gnpdrm.pkg.

The workflow validates PKG magic and uploads the resulting files as an Actions artifact.

## PSN independence

The application does not initialize NP / PSN services. The intended online identity system is a separate guest/custom account system. A later backend integration must use HTTPS/WSS or a properly secured peer-to-peer transport; it must not rely on PSN sign-in.

## Planned next stages

1. Provision a dedicated zero-cost Supabase project with the user-authorized account.
2. Apply the SQL schema and RLS policies in supabase/migrations.
3. Implement authenticated room creation/join and presence.
4. Implement real-time gameplay transport and NAT fallback.
5. Replace the local arena test with original, non-infringing game logic or integrate user-owned game data.
6. Test on RPCS3 and physical PS3 hardware.
