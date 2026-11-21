/*
 * The PS3 LV2 PRX loader does not support the PPU TLS relocations used by
 * newlib's per-thread errno accessor. This module's networking and lobby
 * calls are serialized on the game thread, so give this PRX its own errno
 * slot instead of importing the TLS-backed accessor.
 */
#include <errno.h>

static int cb_ps3_errno;

int *__errno(void)
{
    return &cb_ps3_errno;
}
