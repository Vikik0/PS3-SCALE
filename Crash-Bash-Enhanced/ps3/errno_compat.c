/*
 * Pair with reent_compat.c so the PRX uses the process-global newlib errno
 * slot rather than newlib's pthread/TLS-backed errno accessor.
 */
#include <sys/reent.h>

static int cb_errno_fallback;

int *__errno(void)
{
    struct _reent *reent = __getreent();
    return reent ? &reent->_errno : &cb_errno_fallback;
}
