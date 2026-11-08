#include <stdint.h>
#include <string.h>
#include <ppu-lv2.h>

#include "ps3scale/hardware_info.h"

static int sys_sm_get_system_info(uint64_t raw[3])
{
    lv2syscall1(387, (uint64_t)raw);
    return_to_user_prog(int);
}

static int sys_dbg_get_console_type(uint64_t *type)
{
    lv2syscall1(985, (uint64_t)type);
    return_to_user_prog(int);
}

int ps3scale_hardware_detect(ps3scale_hardware_info_t *out)
{
    uint64_t raw[3] = {0, 0, 0};
    uint64_t console_type = 0;
    int result;

    if (!out) return -1;
    result = sys_sm_get_system_info(raw);
    if (result != 0) return result;

    result = ps3scale_hardware_info_parse((const uint8_t *)raw,
                                          sizeof(raw), out);
    if (result != 0) return result;

    /*
     * Optional read-only console-type query. Some CFW/HEN environments
     * restrict syscall 985, so failure leaves the type as UNKNOWN.
     */
    if (sys_dbg_get_console_type(&console_type) == 0 &&
        console_type >= PS3SCALE_CONSOLE_CEX &&
        console_type <= PS3SCALE_CONSOLE_DEH) {
        out->console_type = (ps3scale_console_type_t)console_type;
    }
    return 0;
}
