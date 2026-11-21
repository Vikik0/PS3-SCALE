#ifndef CRASH_BASH_PS3_TIME_UTIL_H
#define CRASH_BASH_PS3_TIME_UTIL_H

#include_next <sys/time_util.h>
#include <sys/lv2_syscall.h>

/* LV2 syscall 149 returns monotonic system time in microseconds. */
static inline uint64_t sys_time_get_system_time(void)
{
    lv2syscall0(149);
    return (uint64_t)p1;
}

#endif
