Crash Bash Online — PS3 installer

INSTALL
1. Install this PKG from Package Manager.
2. Open "Crash Bash Online" from the XMB.
3. Select YES to add the VSH loader to /dev_hdd0/boot_plugins.txt.
4. Reboot the PS3.
5. Start your own legally obtained supported Crash Bash (USA) copy.
6. Press SELECT + L3 + R3 in the game to open the online menu.
7. Enter your deployed Worker hostname (without https://) and port 443.
8. Host a room and share the six-character code, or join with a friend's code. A browser player can also share the code of a public room.

REQUIREMENTS
- Cobra-compatible CFW/HEN with boot_plugins.txt and PS3MAPI support.
- Internet access and a correct PS3 date/time for TLS certificate validation.
- A legally obtained compatible Crash Bash USA game copy (SCUS_945.70).
- A deployed Crash Bash Online Worker endpoint.
- Players must use matching supported game builds.

IMPORTANT
- No PSN sign-in is used.
- This package contains the loader and input bridge only. It does not contain the game, BIOS, disc image, or copyrighted game assets.
- The browser website currently provides matchmaking only; browser-to-PS3 gameplay is not available yet.
- Physical PS3 installation, process injection, and gameplay still require hardware verification.

TO DISABLE
Remove this exact line from /dev_hdd0/boot_plugins.txt and reboot:
/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_loader.fake.sprx