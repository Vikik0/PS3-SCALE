# PS3 Hardware and System Detection

## Read-only data sources

PS3 SCALE uses two read-only GameOS queries when the running firmware allows them:

- LV2 syscall 387 (`sys_sm_get_system_info`) supplies the 24-byte `platform_info_s` record: a 7-byte firmware field, a 9-byte Platform ID field (numeric code at offset 7, ASCII name beginning at offset 8), a 32-bit build number, and a reserved 32-bit field.
- LV2 syscall 985 (`sys_dbg_get_console_type`) is an optional best-effort query. It writes the console type through its output pointer: CEX=1, DEX=2, DEH=3.

The app does not read or display IDPS, PSID, account identifiers, or other console-unique identifiers.

## Platform ID mapping

The Platform ID identifies a hardware platform / board revision. PS3 SCALE maps known IDs to a retail model family and series:

- FAT: Cok14 (CECHA/B), CokB10 (CECHC/E), CokC12 (CECHG), CokD10 (CECHH), CokE10 (CECHJ/K), CokF10 (CECHL/M/P/Q)
- Slim: CokG11 (20xx), CokH11 (21xx), CokJ13/J20 (25xx), CokK10 (30xx)
- Super Slim: CokM10/M20/M30 (40xx), CokP10/P20/P30/P40 (42xx), CokR10/R20/R30/R40 (43xx)
- Additional N-series IDs (CokN10/N20/N30/N40) are identified as Super Slim platform revisions, but their exact retail model mapping is not asserted.

This is a series-level identification, not a guaranteed exact retail SKU. Some board revisions (for example M40, N-series, and later R-series variants) have incomplete public retail mappings; the app displays the raw Platform ID so the user can distinguish them. It cannot reliably determine the region letter or every A/B suffix from Platform ID alone. Unknown IDs are shown as unknown rather than guessed.

## Failure behavior

Some CFW/HEN builds restrict one or both syscalls. If syscall 387 is unavailable, the About page reports hardware information as unavailable and the application continues normally. If syscall 985 is unavailable, the console type remains UNKNOWN; firmware and Platform ID information can still be shown.

The detection result is informational and is not used to apply game patches. A graphics profile must still match the exact game Title ID, game update, and verified patch data before it can be applied.

## Verification

Host tests cover firmware-version parsing and representative FAT, Slim, and Super Slim Platform IDs using the correct 7-byte firmware / 9-byte Platform ID layout. Actual syscall availability and the displayed result must be checked on the user's PS3 CFW 4.92 device.
