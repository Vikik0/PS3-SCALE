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

#define SYS_PRX_RESIDENT 0

int module_start(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    return cb_input_install() == 0 ? SYS_PRX_RESIDENT : -1;
}

int module_stop(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    cb_input_remove();
    return SYS_PRX_RESIDENT;
}
