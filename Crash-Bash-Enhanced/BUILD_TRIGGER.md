# Crash Bash Online — PS3 build workflow

The workflow `.github/workflows/crash-bash-build.yml` runs on pushes to
`crash-bash-enhanced`, matching pull requests, and manual dispatch.

## Build scope

The installable PKG is built from:

- RetroArch's PSL1GHT PS3 frontend with Netplay enabled.
- A statically linked PCSX-ReARMed PS1 core.
- Port 1 Multitap enabled in the core.
- RetroArch Netplay lockstep enabled to reduce the memory cost of the
  0x440000-byte PS1 savestate.
- Public lobby announcement, NAT traversal and the Madrid relay endpoint.

The earlier Tiny3D shell, Vercel room API and custom UDP input codec are separate
prototypes; they are not the gameplay path in this PKG.

## CI checks

1. Bash syntax check for the PS3 package build script.
2. Host-side protocol, JSON, archive, netcode and UDP peer tests.
3. Web lobby token/security tests.
4. PS3 cross-build, SELF/SFO validation, PKG creation and SHA-256 generation.

The package and manifest are uploaded as the `Crash-Bash-Online-PS3-PKG`
workflow artifact.

A green build is not a hardware certification. RetroArch's upstream Netplay FAQ
warns that PSX netplay can be too demanding; lockstep is a mitigation, and
two-console testing on real PS3 hardware is still required.
