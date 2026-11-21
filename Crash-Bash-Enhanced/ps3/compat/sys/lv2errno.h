#ifndef CRASH_BASH_PS3_SYS_LV2ERRNO_H
#define CRASH_BASH_PS3_SYS_LV2ERRNO_H

#include <reent.h>
#include <errno.h>
#include <ppu-types.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 lv2error(s32 error);
s32 lv2errno(s32 error);
s32 lv2errno_r(struct _reent *reent, s32 error);

#ifdef __cplusplus
}
#endif

#endif
