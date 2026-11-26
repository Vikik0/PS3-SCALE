#include "ps3scale/ps2_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8_t expected_all[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x44, 0x00, 0x00, 0x00,
        0x46, 0x00, 0x00, 0x00,
        0x48, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x20, 0x4E, 0x00, 0x00
    };
    static const uint8_t expected_frame_pacing[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x48, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x20, 0x4E, 0x00, 0x00
    };
    uint8_t data[64];
    size_t size = 0;

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H |
        PS3SCALE_PS2_CFG_FRAME_PACING,
        &size) == 0);
    assert(size == sizeof(expected_all));
    assert(memcmp(data, expected_all, sizeof(expected_all)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    assert(size == sizeof(expected_frame_pacing));
    assert(memcmp(data, expected_frame_pacing,
                  sizeof(expected_frame_pacing)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING, &size) == 0);
    assert(size == 12);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(data, sizeof(data), 0, &size) < 0);
    assert(ps3scale_ps2_config_build(data, sizeof(data), 0x80, &size) < 0);
    assert(ps3scale_ps2_config_build(data, 8,
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING, &size) < 0);

    data[8] = 0x45;
    assert(ps3scale_ps2_config_validate_generated(data, 12) < 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data), PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    data[12] = 0x02;
    assert(ps3scale_ps2_config_validate_generated(data, size) < 0);
    assert(ps3scale_ps2_config_build(
        data, sizeof(data), PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    data[16] = 0x21;
    assert(ps3scale_ps2_config_validate_generated(data, size) < 0);

    {
        static const uint8_t external_config[] = {
            0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
            0x44, 0x00, 0x00, 0x00
        };
        static const uint8_t external_enc[] = {
            0x50, 0x53, 0x32, 0x00, 0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
        };
        assert(ps3scale_ps2_config_validate_external(
            external_config, sizeof(external_config), "CUSTOM") == 0);
        assert(ps3scale_ps2_config_validate_external(
            external_enc, sizeof(external_enc), "ENC") == 0);
        assert(ps3scale_ps2_config_validate_external(
            external_config, sizeof(external_config), "UNKNOWN") < 0);
    }

    puts("PS3 SCALE PS2 CONFIG builder tests: PASS");
    return 0;
}
