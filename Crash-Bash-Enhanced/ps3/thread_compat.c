#include <ppu-asm.h>
#include <ppu-types.h>
#include <sys/thread.h>

/* PSL1GHT's liblv2 wrapper around the exported sysThreadCreateEx entry.
 * PS3DK's PRX runtime includes pthread.c but does not ship this thin wrapper. */
extern s32 sysThreadCreateEx(sys_ppu_thread_t *threadid, opd32 *opdentry,
                             void *arg, s32 priority, u64 stacksize,
                             u64 flags, char *threadname);

s32 sysThreadCreate(sys_ppu_thread_t *threadid, void (*entry)(void *),
                    void *arg, s32 priority, u64 stacksize, u64 flags,
                    char *threadname)
{
    return sysThreadCreateEx(threadid, (opd32 *)__get_opd32(entry), arg,
                             priority, stacksize, flags, threadname);
}

