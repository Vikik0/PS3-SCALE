/*
 * Crash Bash Enhanced PS3 input bridge prototype.
 *
 * Load this module into the PS1 emulator process only. The bridge starts
 * inactive and does not change controller enumeration until the network
 * layer explicitly enables it.
 */
#include <stddef.h>
#include <stdint.h>

#include "input_hook.h"
#include "net_adapter.h"
#include "tls_client.h"

#define SYS_PRX_RESIDENT 0

extern void __libc_init_array(void);
extern void __libc_fini_array(void);
static int runtime_initialized;

int module_start(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    if (!runtime_initialized) {
        __libc_init_array();
        runtime_initialized = 1;
    }
    if (cb_input_install() != 0) return -1;
    (void)cb_net_init();
    return SYS_PRX_RESIDENT;
}

int module_stop(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    cb_net_leave();
    cb_tls_global_cleanup();
    cb_input_remove();
    if (runtime_initialized) {
        __libc_fini_array();
        runtime_initialized = 0;
    }
    return SYS_PRX_RESIDENT;
}
