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

static int looks_like_title_id(const uint8_t *text, size_t length)
{
    size_t i;
    unsigned letters = 0;
    unsigned digits = 0;

    if (!text || length < 9u || length > 12u)
        return 0;

    for (i = 0; i < length; ++i) {
        uint8_t c = text[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            ++letters;
        } else if (c >= '0' && c <= '9') {
            ++digits;
        } else if (c != '_' && c != '-' && c != '.') {
            return 0;
        }
    }

    return letters >= 3u && digits >= 4u;
}

static int patches_valid(const ps3scale_ps2_code_patch_t *patches,
                         size_t patch_count)
{
    size_t i;
    size_t j;

    if (patch_count > PS3SCALE_PS2_CONFIG_MAX_CODE_PATCHES ||
        (patch_count != 0 && !patches))
        return 0;

    for (i = 0; i < patch_count; ++i) {
        if ((patches[i].ee_address & 3u) != 0 ||
            patches[i].ee_address >= 0x10000000u ||
            patches[i].original_word == patches[i].replacement_word)
            return 0;
        for (j = 0; j < i; ++j) {
            if (patches[i].ee_address == patches[j].ee_address)
                return 0;
        }
    }
    return 1;
}

static int write_patch_command(uint8_t *out, size_t capacity,
                               const ps3scale_ps2_code_patch_t *patches,
                               size_t patch_count, size_t *written)
{
    size_t needed;
    size_t pos;
    size_t i;

    if (written)
        *written = 0;
    if (!out || !written || patch_count == 0 ||
        !patches_valid(patches, patch_count))
        return -1;

    needed = 8u + patch_count * 12u;
    if (capacity < needed)
        return -2;

    out[0] = 0x0A;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = 0x00;
    write_u32_le(out + 4u, (uint32_t)patch_count);
    pos = 8u;

    for (i = 0; i < patch_count; ++i) {
        write_u32_le(out + pos, patches[i].ee_address);
        write_u32_le(out + pos + 4u, patches[i].original_word);
        write_u32_le(out + pos + 8u, patches[i].replacement_word);
        pos += 12u;
    }

    *written = pos;
    return 0;
}

int ps3scale_ps2_config_build_with_patches(
    uint8_t *out, size_t capacity, unsigned flags,
    const ps3scale_ps2_code_patch_t *patches, size_t patch_count,
    size_t *out_size)
{
    size_t needed = sizeof(ps2_config_header) + 4u;
    size_t pos;
    const unsigned allowed =
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H |
        PS3SCALE_PS2_CFG_FRAME_PACING |
        PS3SCALE_PS2_CFG_ENABLE_PRESSURE;

    if (out_size)
        *out_size = 0;
    if (!out || !out_size || (flags & ~allowed) != 0 ||
        (flags == 0 && patch_count == 0) ||
        !patches_valid(patches, patch_count))
        return -1;

    if (has_flag(flags, PS3SCALE_PS2_CFG_DISABLE_SMOOTHING))
        needed += 4u;
    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_L2H))
        needed += 4u;
    if (has_flag(flags, PS3SCALE_PS2_CFG_FRAME_PACING))
        needed += 12u;
    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_PRESSURE))
        needed += 4u;
    if (patch_count != 0)
        needed += 8u + patch_count * 12u;

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

    if (has_flag(flags, PS3SCALE_PS2_CFG_ENABLE_PRESSURE)) {
        out[pos++] = 0x50;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
        out[pos++] = 0x00;
    }

    if (patch_count != 0) {
        size_t patch_written = 0;
        if (write_patch_command(out + pos, capacity - pos, patches,
                                patch_count, &patch_written) != 0)
            return -3;
        pos += patch_written;
    }

    /* Empty 0x00 command terminates the CONFIG command stream. */
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;

    *out_size = pos;
    return 0;
}

int ps3scale_ps2_config_build(uint8_t *out, size_t capacity,
                              unsigned flags, size_t *out_size)
{
    return ps3scale_ps2_config_build_with_patches(
        out, capacity, flags, NULL, 0, out_size);
}

int ps3scale_ps2_config_append_patches(
    const uint8_t *base, size_t base_size,
    const ps3scale_ps2_code_patch_t *patches, size_t patch_count,
    uint8_t *out, size_t capacity, size_t *out_size)
{
    size_t payload_size;
    size_t patch_size;
    size_t needed;
    size_t i;

    if (out_size)
        *out_size = 0;
    if (!base || !out || !out_size ||
        base_size < sizeof(ps2_config_header) + 4u ||
        base_size > PS3SCALE_PS2_CONFIG_MAX_BYTES ||
        memcmp(base, ps2_config_header, sizeof(ps2_config_header)) != 0 ||
        read_u32_le(base + base_size - 4u) != 0 ||
        !patches_valid(patches, patch_count) || patch_count == 0)
        return -1;

    payload_size = base_size - 4u;

    /* A Title ID / multidisc command must remain the final command. */
    for (i = sizeof(ps2_config_header); i + 13u <= payload_size; i += 4u) {
        size_t id_length;
        if (read_u32_le(base + i) == 0) {
            for (id_length = 9u; id_length <= 12u &&
                 i + 4u + id_length <= payload_size; ++id_length) {
                if (looks_like_title_id(base + i + 4u, id_length))
                    return -2;
            }
        }
        if (read_u32_le(base + i) == 0x0Au)
            return -3;
    }

    patch_size = 8u + patch_count * 12u;
    needed = payload_size + patch_size + 4u;
    if (needed > capacity || needed > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -4;

    memcpy(out, base, payload_size);
    if (write_patch_command(out + payload_size, capacity - payload_size,
                            patches, patch_count, &patch_size) != 0)
        return -5;
    payload_size += patch_size;
    out[payload_size++] = 0x00;
    out[payload_size++] = 0x00;
    out[payload_size++] = 0x00;
    out[payload_size++] = 0x00;

    *out_size = payload_size;
    return 0;
}

int ps3scale_ps2_config_validate_generated(const uint8_t *data, size_t size)
{
    size_t pos;
    size_t command_end;
    unsigned seen = 0;

    if (!data || size < sizeof(ps2_config_header) + 4u ||
        size > PS3SCALE_PS2_CONFIG_MAX_BYTES)
        return -1;

    if (memcmp(data, ps2_config_header, sizeof(ps2_config_header)) != 0)
        return -2;
    if (read_u32_le(data + size - 4u) != 0)
        return -9;

    command_end = size - 4u;
    pos = sizeof(ps2_config_header);
    while (pos < command_end) {
        unsigned flag;
        uint8_t command;

        if (command_end - pos < 4u)
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

            if (command_end - pos < 12u)
                return -3;
            mode = read_u32_le(data + pos + 4u);
            delay = read_u32_le(data + pos + 8u);
            if (mode != PS3SCALE_PS2_CFG_VSYNC_MODE_NO_IPU ||
                delay != PS3SCALE_PS2_CFG_VSYNC_DELAY_20MS)
                return -8;
            flag = PS3SCALE_PS2_CFG_FRAME_PACING;
            pos += 12u;
        } else if (command == 0x50) {
            flag = PS3SCALE_PS2_CFG_ENABLE_PRESSURE;
            pos += 4u;
        } else if (command == 0x0A) {
            uint32_t count;
            size_t patch_pos;
            size_t i;
            size_t j;

            if (command_end - pos < 8u)
                return -3;
            count = read_u32_le(data + pos + 4u);
            if (count == 0 ||
                count > PS3SCALE_PS2_CONFIG_MAX_CODE_PATCHES ||
                command_end - pos < 8u + (size_t)count * 12u)
                return -3;

            patch_pos = pos + 8u;
            for (i = 0; i < count; ++i) {
                uint32_t address = read_u32_le(data + patch_pos);
                uint32_t original = read_u32_le(data + patch_pos + 4u);
                uint32_t replacement = read_u32_le(data + patch_pos + 8u);

                if ((address & 3u) != 0 || address >= 0x10000000u ||
                    original == replacement)
                    return -10;
                for (j = 0; j < i; ++j) {
                    uint32_t prior = read_u32_le(
                        data + patch_pos - (i - j) * 12u);
                    if (prior == address)
                        return -11;
                }
                patch_pos += 12u;
            }
            flag = 0x10u;
            pos += 8u + (size_t)count * 12u;
        } else {
            return -5;
        }

        if (seen & flag)
            return -6;
        seen |= flag;
    }

    return pos == command_end && seen ? 0 : -7;
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
