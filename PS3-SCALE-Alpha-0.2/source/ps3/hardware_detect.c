#include <stdint.h>
#include <string.h>
#include <ppu-lv2.h>

#include "ps3scale/hardware_info.h"

static int sys_sm_get_system_info(uint64_t raw[3])
{
    lv2syscall1(387, (uint64_t)raw);
    return_to_user_prog(int);
}

static ps3scale_console_type_t sys_dbg_get_console_type(void)
{
    uint64_t type = 0;

    /*
     * Syscall 985 writes the console type to its output pointer.
     * The returned value is CEX=1, DEX=2, or DEH=3; zero means unavailable.
     */
    lv2syscall1(985, (uint64_t)&type);
    if (type < PS3SCALE_CONSOLE_CEX ||
        type > PS3SCALE_CONSOLE_DEH)
        return PS3SCALE_CONSOLE_UNKNOWN;

    return (ps3scale_console_type_t)type;
}

int ps3scale_hardware_detect(ps3scale_hardware_info_t *out)
{
    uint64_t raw[3] = {0, 0, 0};
    int result;

    if (!out) return -1;
    result = sys_sm_get_system_info(raw);
    if (result != 0) return result;

    result = ps3scale_hardware_info_parse((const uint8_t *)raw,
                                          sizeof(raw), out);
    if (result != 0) return result;

    /*
     * Optional read-only console-type query. If syscall 985 is unavailable,
     * its zero-initialized output leaves the type as UNKNOWN.
     */
    out->console_type = sys_dbg_get_console_type();
    return 0;
}
