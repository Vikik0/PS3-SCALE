/*
 * The PS3 LV2 PRX loader does not support the PPU TLS relocations used by
 * newlib's per-thread errno implementation. This module's networking and
 * lobby calls are serialized on the game thread, so give this PRX its own
 * errno slot rather than importing newlib's TLS-backed __errno().
 */
#include <errno.h>

static int cb_ps3_errno;

int *__errno(void)
{
    return &cb_ps3_errno;
}
