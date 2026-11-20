/*
 * Ghosts Campaign Co-op PRX bootstrap.
 *
 * This module is deliberately passive: it proves the PS3 PRX build pipeline
 * and module lifecycle only. No game memory is read or written, and no
 * Campaign actor hooks are installed until the exact title ID / TU executable
 * has been analysed and the candidate routines have been verified.
 */
#include <stddef.h>
#include <stdint.h>

#define SYS_PRX_RESIDENT 0

static volatile uint32_t g_module_state = 0;
static volatile uint32_t g_start_count = 0;

int module_start(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    g_start_count++;
    g_module_state = 1;
    return SYS_PRX_RESIDENT;
}

int module_stop(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    g_module_state = 0;
    return SYS_PRX_RESIDENT;
}

/* Kept as local diagnostics for the next exported debug interface. */
uint32_t ghc_module_state(void)
{
    return g_module_state;
}

uint32_t ghc_module_start_count(void)
{
    return g_start_count;
}
