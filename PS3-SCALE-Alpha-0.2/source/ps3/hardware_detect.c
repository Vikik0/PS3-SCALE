#include <stdint.h>
#include <string.h>
#include <ppu-lv2.h>

#include "ps3scale/hardware_info.h"

static int sys_sm_get_system_info(uint64_t raw[3])
{
    lv2syscall1(387, (uint64_t)raw);
    return_to_user_prog(int);
}

int ps3scale_hardware_detect(ps3scale_hardware_info_t *out)
{
    uint64_t raw[3] = {0, 0, 0};
    int result;

    if (!out) return -1;
    result = sys_sm_get_system_info(raw);
    if (result != 0) return result;

    return ps3scale_hardware_info_parse((const uint8_t *)raw,
                                        sizeof(raw), out);
}
