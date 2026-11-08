# PS3 Hardware and System Detection

## Read-only data sources

PS3 SCALE uses two read-only GameOS queries when the running firmware allows them:

- LV2 syscall 387 (`sys_sm_get_system_info`) supplies the system-software version and Platform ID.
- LV2 syscall 985 (`sys_dbg_get_console_type`) is an optional best-effort query for CEX, DEX, or DEH.

The app does not read or display IDPS, PSID, account identifiers, or other console-unique identifiers.

## Platform ID mapping

The Platform ID identifies a hardware platform / board revision. PS3 SCALE maps known IDs to a retail model family and series:

- FAT families: Cok14, CokB10, CokC12, CokD10, CokE10, CokF10
- Slim families: CokG11 (20xx), CokH11 (21xx), CokJ13/J20 (25xx), CokK10 (30xx)
- Super Slim families: CokM10/M20/M30/M40 (40xx), CokP10/P20/P30/P40 (42xx), CokR10 (43xx)

This is a series-level identification, not a guaranteed exact retail SKU. It cannot reliably determine the region letter or every A/B suffix from Platform ID alone. Unknown IDs are shown as unknown rather than guessed.

## Failure behavior

Some CFW/HEN builds restrict one or both syscalls. If syscall 387 is unavailable, the About page reports hardware information as unavailable and the application continues normally. If syscall 985 is unavailable, the console type remains UNKNOWN; firmware and Platform ID information can still be shown.

The detection result is informational and is not used to apply game patches. A graphics profile must still match the exact game Title ID, game update, and verified patch data before it can be applied.

## Verification

Host tests cover firmware-version parsing and representative FAT, Slim, and Super Slim Platform IDs. Actual syscall availability and the displayed result must be checked on the user's PS3 CFW 4.92 device.
