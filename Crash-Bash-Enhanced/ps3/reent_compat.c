/*
 * PS3 PRX single-thread reentrancy shim.
 *
 * The PS3 PRX loader does not support the TLS relocations used by PS3DK's
 * pthread-based __getreent implementation. This plugin serializes its own
 * libc/network work on the game thread, so use the process-global newlib
 * reentrancy object instead of pthread_getspecific().
 */
#include <sys/reent.h>

struct _reent *__getreent(void)
{
    if (_impure_ptr) return _impure_ptr;
    return (struct _reent *)_global_impure_ptr;
}
