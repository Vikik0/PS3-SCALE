#include "ps3scale/ps2_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8_t expected[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x44, 0x00, 0x00, 0x00,
        0x46, 0x00, 0x00, 0x00
    };
    uint8_t data[64];
    size_t size = 0;

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H,
        &size) == 0);
    assert(size == sizeof(expected));
    assert(memcmp(data, expected, sizeof(expected)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING,
        &size) == 0);
    assert(size == 12);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(data, sizeof(data), 0, &size) < 0);
    assert(ps3scale_ps2_config_build(data, sizeof(data), 0x80, &size) < 0);
    assert(ps3scale_ps2_config_build(data, 8,
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING, &size) < 0);

    data[8] = 0x45;
    assert(ps3scale_ps2_config_validate_generated(data, 12) < 0);

    puts("PS3 SCALE PS2 CONFIG builder tests: PASS");
    return 0;
}
