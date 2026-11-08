# Device test matrix

Primary device:
- Model: CECH-2504A (PS3 Slim)
- Firmware: 4.93
- CFW: CEX
- Cobra: 8.5
- Available HDD space reported: about 10 GB

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
