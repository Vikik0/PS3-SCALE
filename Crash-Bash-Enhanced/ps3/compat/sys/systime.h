#ifndef CRASH_BASH_PS3_SYS_SYSTIME_H
#define CRASH_BASH_PS3_SYS_SYSTIME_H

#include <ppu-lv2.h>
#include <lv2/systime.h>

#ifdef __cplusplus
extern "C" {
#endif

LV2_INLINE u64 sysGetTimebaseFrequency(void)
{
    lv2syscall0(147);
    return_to_user_prog(u64);
}

LV2_SYSCALL sysGetCurrentTime(u64 *sec, u64 *nsec)
{
    lv2syscall2(145, (u64)sec, (u64)nsec);
    return_to_user_prog(s32);
}

LV2_SYSCALL sysSetCurrentTime(u64 sec, u64 nsec)
{
    lv2syscall2(146, sec, nsec);
    return_to_user_prog(s32);
}

LV2_SYSCALL sysSleep(u32 seconds)
{
    lv2syscall1(142, seconds);
    return_to_user_prog(s32);
}

LV2_SYSCALL sysUsleep(u32 useconds)
{
    lv2syscall1(141, useconds);
    return_to_user_prog(s32);
}

/* Compatibility helper expected by PS3DK's POSIX clock_gettime runtime. */
LV2_INLINE u64 sysGetSystemTime(void)
{
    u64 sec = 0;
    u64 nsec = 0;
    (void)sysGetCurrentTime(&sec, &nsec);
    return sec * 1000000ULL + nsec / 1000ULL;
}

#ifdef __cplusplus
}
#endif

#endif
