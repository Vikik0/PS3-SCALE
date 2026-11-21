Crash Bash Online — PS3 installer

1. Install this PKG from Package Manager.
2. Open "Crash Bash Online" from the XMB.
3. Select Yes when asked to enable the VSH loader.
4. Reboot the PS3.

The installer appends this line to /dev_hdd0/boot_plugins.txt:
  /dev_hdd0/game/CBAS00001/USRDIR/crash_bash_loader.fake.sprx

Requirements:
- Cobra-compatible CFW or HEN with boot_plugins.txt support.
- A supported Crash Bash game image and the project's matching game hooks.
- Internet access for the online relay.

The package contains the loader and input bridge in this title's USRDIR.
