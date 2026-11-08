#include "ps3scale/ps2_config.h"

#include <string.h>

static const uint8_t ps2_config_header[8] = {
    0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00
};

static int has_flag(unsigned flags, unsigned flag)
{
    return (flags & flag) != 0;
}

int ps3scale_ps2_config_build(uint8_t *out, size_t capacity,
                              unsigned flags, size_t *out_size)
{
    size_t pos = sizeof(ps2_config_header);
    const unsigned allowed =
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H;

    if (out_size) *out_size = 0;
    if (!out || !out_size || flags == 0 || (flags & ~allowed) != 0)
        return -1;

    if (capacity < sizeof(ps2_config_header) ||
        capacity > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -2;

    if (has_flag(flags, PS3SCALE_PS2_CFG_DISABLE_SMOOTHING))
        pos += 4;
    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_L2H))
        pos += 4;

    if (pos > capacity || pos > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -3;

    memcpy(out, ps2_config_header, sizeof(ps2_config_header));
    pos = sizeof(ps2_config_header);

    if (has_flag(flags, PS3SCALE_PS2_CFG_DISABLE_SMOOTHING)) {
        out[pos++] = 0x44;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
    }

    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_L2H)) {
        out[pos++] = 0x46;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
    }

    *out_size = pos;
    return 0;
}

int ps3scale_ps2_config_validate_generated(const uint8_t *data, size_t size)
{
    size_t pos;
    unsigned seen = 0;

    if (!data || size < sizeof(ps2_config_header) ||
        size > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -1;

    if (memcmp(data, ps2_config_header, sizeof(ps2_config_header)) != 0)
        return -2;

    if (((size - sizeof(ps2_config_header)) % 4u) != 0)
        return -3;

    for (pos = sizeof(ps2_config_header); pos < size; pos += 4) {
        unsigned flag;

        if (data[pos + 1] != 0 || data[pos + 2] != 0 || data[pos + 3] != 0)
            return -4;

        if (data[pos] == 0x44)
            flag = PS3SCALE_PS2_CFG_DISABLE_SMOOTHING;
        else if (data[pos] == 0x46)
            flag = PS3SCALE_PS2_CFG_ENABLE_L2H;
        else
            return -5;

        if (seen & flag)
            return -6;
        seen |= flag;
    }

    return seen ? 0 : -7;
}
