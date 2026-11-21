/*! \file lv2/thread.h
 \brief PPU thread library compatibility declarations.

 This header mirrors the minimal PSL1GHT ABI declarations required by
 PS3DK's sys/thread.h runtime wrapper.
*/
#ifndef __LV2_THREAD_H__
#define __LV2_THREAD_H__

#include <ppu-types.h>

#ifdef __cplusplus
extern "C" {
#endif

void sysThreadExit(u64 ret_val);
s32 sysThreadGetId(sys_ppu_thread_t *threadid);

#ifdef __cplusplus
}
#endif

#endif
