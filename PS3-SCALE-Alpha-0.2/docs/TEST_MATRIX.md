# Device test matrix

Primary device (user-reported):
- Family: PS3 Slim
- System software: CFW 4.92
- Exact CECH retail model: not yet confirmed
- CEX / DEX: not yet verified on the console; the app now queries this read-only when supported
- Cobra version and available HDD space: not yet confirmed

The app's About page reports the Platform ID, mapped hardware family/series, firmware, and console type. This is a read-only identification step; the user must still confirm the on-device values before a hardware-specific feature is enabled.

Initial safety checks:
1. Install only the manager PKG; do not install any game patch in Alpha 0.2.
2. Launch the app and confirm it returns to XMB with Circle.
3. Run scan twice and confirm the library count is stable (no duplicates).
4. Confirm folders, ISO files, and PARAM.SFO files remain unchanged.
5. Test with HDD only, then with one USB device.
6. Keep at least 1 GB free on the internal HDD; large future patch/audio packs can live on USB.

Other hardware targets after the Slim test:
- compatible CFW FAT
- other CFW Slim revisions
- compatible CFW Super Slim (where available; most Super Slim models do not support traditional CFW)
