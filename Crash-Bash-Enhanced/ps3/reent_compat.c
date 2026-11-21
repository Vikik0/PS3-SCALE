/*
 * PS3 PRX single-thread reentrancy shim.
 *
 * The PS3 PRX loader does not support the TLS relocations used by PS3DK's
 * pthread-backed __getreent implementation. The relay/menu calls in this
 * PRX are serialized on the game thread, so use newlib's process-global
 * reentrancy object instead of pthread_getspecific().
 */
#include <sys/reent.h>

static struct _reent cb_fallback_reent;

struct _reent *__getreent(void)
{
    return _impure_ptr ? _impure_ptr : &cb_fallback_reent;
}
