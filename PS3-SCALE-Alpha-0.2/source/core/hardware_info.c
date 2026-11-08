#include "ps3scale/hardware_info.h"

#include <stdio.h>
#include <string.h>

typedef struct platform_model_entry {
    const char *platform_id;
    const char *model_series;
    ps3scale_hardware_family_t family;
} platform_model_entry_t;

static const platform_model_entry_t platform_models[] = {
    {"Cok14", "PS3 FAT (CECHA / CECHB)", PS3SCALE_HARDWARE_FAT},
    {"CokB10", "PS3 FAT (CECHC / CECHE)", PS3SCALE_HARDWARE_FAT},
    {"CokC12", "PS3 FAT (CECHG)", PS3SCALE_HARDWARE_FAT},
    {"CokD10", "PS3 FAT (CECHH)", PS3SCALE_HARDWARE_FAT},
    {"CokE10", "PS3 FAT (CECHJ / CECHK)", PS3SCALE_HARDWARE_FAT},
    {"CokF10", "PS3 FAT (CECHL / CECHP)", PS3SCALE_HARDWARE_FAT},
    {"CokG11", "PS3 Slim (CECH-20xx)", PS3SCALE_HARDWARE_SLIM},
    {"CokH11", "PS3 Slim (CECH-21xx)", PS3SCALE_HARDWARE_SLIM},
    {"CokJ13", "PS3 Slim (CECH-25xx / JTP-001)", PS3SCALE_HARDWARE_SLIM},
    {"CokJ20", "PS3 Slim (CECH-25xx / JSD-001)", PS3SCALE_HARDWARE_SLIM},
    {"CokK10", "PS3 Slim (CECH-30xx)", PS3SCALE_HARDWARE_SLIM},
    {"CokM10", "PS3 Super Slim (CECH-40xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokM20", "PS3 Super Slim (CECH-40xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokM30", "PS3 Super Slim (CECH-40xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokM40", "PS3 Super Slim (CECH-40xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokP10", "PS3 Super Slim (CECH-42xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokP20", "PS3 Super Slim (CECH-42xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokP30", "PS3 Super Slim (CECH-42xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokP40", "PS3 Super Slim (CECH-42xx)", PS3SCALE_HARDWARE_SUPER_SLIM},
    {"CokR10", "PS3 Super Slim (CECH-43xx)", PS3SCALE_HARDWARE_SUPER_SLIM}
};

const char *ps3scale_hardware_family_name(ps3scale_hardware_family_t family)
{
    switch (family) {
        case PS3SCALE_HARDWARE_FAT: return "FAT";
        case PS3SCALE_HARDWARE_SLIM: return "SLIM";
        case PS3SCALE_HARDWARE_SUPER_SLIM: return "SUPER SLIM";
        case PS3SCALE_HARDWARE_TOOL: return "TOOL / DEBUG";
        default: return "UNKNOWN";
    }
}

const char *ps3scale_console_type_name(ps3scale_console_type_t type)
{
    switch (type) {
        case PS3SCALE_CONSOLE_CEX: return "CEX";
        case PS3SCALE_CONSOLE_DEX: return "DEX";
        case PS3SCALE_CONSOLE_DEH: return "DEH";
        default: return "UNKNOWN";
    }
}

int ps3scale_hardware_info_parse(const uint8_t *raw, size_t raw_size,
                                 ps3scale_hardware_info_t *out)
{
    unsigned major, minor_tens, minor_ones;
    size_t i;

    if (!raw || !out || raw_size < 24u)
        return -1;

    memset(out, 0, sizeof(*out));

    /* The first three BCD-like nibbles encode e.g. 03 05 50 -> 3.55. */
    major = (unsigned)(raw[0] & 0x0fu);
    minor_tens = (unsigned)(raw[1] & 0x0fu);
    minor_ones = (unsigned)((raw[2] >> 4) & 0x0fu);
    if (major > 9u || minor_tens > 9u || minor_ones > 9u)
        return -1;

    (void)snprintf(out->firmware_version, sizeof(out->firmware_version),
                   "%u.%u%u", major, minor_tens, minor_ones);

    /*
     * The syscall's documented platform_info_s layout is:
     * firmware_version[7], platform_id[9], build_number, unknown.
     * Platform ID therefore starts at byte 7 (not byte 8).
     */
    for (i = 0; i < sizeof(out->platform_id) - 1u; i++) {
        unsigned char ch = raw[7u + i];
        if (ch == 0) break;
        if (ch < 0x20u || ch > 0x7eu) break;
        out->platform_id[i] = (char)ch;
    }
    out->platform_id[i] = '\0';

    out->build_number =
        ((uint32_t)raw[16] << 24) |
        ((uint32_t)raw[17] << 16) |
        ((uint32_t)raw[18] << 8) |
        (uint32_t)raw[19];

    for (i = 0; i < sizeof(platform_models) /
                    sizeof(platform_models[0]); i++) {
        if (strcmp(out->platform_id, platform_models[i].platform_id) == 0) {
            out->family = platform_models[i].family;
            strncpy(out->model_series, platform_models[i].model_series,
                    sizeof(out->model_series) - 1u);
            return 0;
        }
    }

    if (strncmp(out->platform_id, "Cyt", 3) == 0 ||
        strncmp(out->platform_id, "Deb", 3) == 0) {
        out->family = PS3SCALE_HARDWARE_TOOL;
        strncpy(out->model_series, "PS3 Tool / Debug hardware",
                sizeof(out->model_series) - 1u);
    } else {
        out->family = PS3SCALE_HARDWARE_UNKNOWN;
        strncpy(out->model_series, "Unknown PS3 hardware revision",
                sizeof(out->model_series) - 1u);
    }
    return 0;
}
