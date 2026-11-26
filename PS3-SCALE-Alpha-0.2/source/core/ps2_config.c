#include "ps3scale/ps2_config.h"

#include <string.h>

static const uint8_t ps2_config_header[8] = {
    0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00
};

static int has_flag(unsigned flags, unsigned flag)
{
    return (flags & flag) != 0;
}

static void write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t read_u32_le(const uint8_t *in)
{
    return (uint32_t)in[0] |
           ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) |
           ((uint32_t)in[3] << 24);
}

int ps3scale_ps2_config_build(uint8_t *out, size_t capacity,
                              unsigned flags, size_t *out_size)
{
    size_t needed = sizeof(ps2_config_header);
    size_t pos;
    const unsigned allowed =
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H |
        PS3SCALE_PS2_CFG_FRAME_PACING;

    if (out_size)
        *out_size = 0;
    if (!out || !out_size || flags == 0 || (flags & ~allowed) != 0)
        return -1;

    if (has_flag(flags, PS3SCALE_PS2_CFG_DISABLE_SMOOTHING))
        needed += 4u;
    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_L2H))
        needed += 4u;
    if (has_flag(flags, PS3SCALE_PS2_CFG_FRAME_PACING))
        needed += 12u;

    if (capacity < needed || capacity > PS3SCALE_PS2_CONFIG_MAX_BYTES ||
        needed > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -2;

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

    if (has_flag(flags, PS3SCALE_PS2_CFG_FRAME_PACING)) {
        out[pos++] = 0x48;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
        write_u32_le(out + pos, PS3SCALE_PS2_CFG_VSYNC_MODE_NO_IPU);
        pos += 4u;
        write_u32_le(out + pos, PS3SCALE_PS2_CFG_VSYNC_DELAY_20MS);
        pos += 4u;
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

    pos = sizeof(ps2_config_header);
    while (pos < size) {
        unsigned flag;
        uint8_t command;

        if (size - pos < 4u)
            return -3;
        if (data[pos + 1u] != 0 || data[pos + 2u] != 0 ||
            data[pos + 3u] != 0)
            return -4;

        command = data[pos];
        if (command == 0x44) {
            flag = PS3SCALE_PS2_CFG_DISABLE_SMOOTHING;
            pos += 4u;
        } else if (command == 0x46) {
            flag = PS3SCALE_PS2_CFG_ENABLE_L2H;
            pos += 4u;
        } else if (command == 0x48) {
            uint32_t mode;
            uint32_t delay;

            if (size - pos < 12u)
                return -3;
            mode = read_u32_le(data + pos + 4u);
            delay = read_u32_le(data + pos + 8u);
            if (mode != PS3SCALE_PS2_CFG_VSYNC_MODE_NO_IPU ||
                delay != PS3SCALE_PS2_CFG_VSYNC_DELAY_20MS)
                return -8;
            flag = PS3SCALE_PS2_CFG_FRAME_PACING;
            pos += 12u;
        } else {
            return -5;
        }

        if (seen & flag)
            return -6;
        seen |= flag;
    }

    return seen ? 0 : -7;
}

int ps3scale_ps2_config_validate_external(const uint8_t *data, size_t size,
                                          const char *type)
{
    if (!data || !type)
        return -1;
    if (strcmp(type, "ENC") == 0) {
        if (size < 16u || size > (PS3SCALE_PS2_CONFIG_MAX_BYTES * 2u) ||
            memcmp(data, "PS2\0", 4) != 0)
            return -2;
        return 0;
    }
    if (strcmp(type, "CUSTOM") != 0 && strcmp(type, "NET") != 0 &&
        strcmp(type, "GX") != 0 && strcmp(type, "SOFT") != 0)
        return -3;
    if (size < sizeof(ps2_config_header) ||
        size > PS3SCALE_PS2_CONFIG_MAX_BYTES ||
        memcmp(data, ps2_config_header, 4) != 0)
        return -4;
    return 0;
}
