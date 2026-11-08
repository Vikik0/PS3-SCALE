#ifndef PS3SCALE_HARDWARE_INFO_H
#define PS3SCALE_HARDWARE_INFO_H

#include <stddef.h>
#include <stdint.h>

typedef enum ps3scale_hardware_family {
    PS3SCALE_HARDWARE_UNKNOWN = 0,
    PS3SCALE_HARDWARE_FAT,
    PS3SCALE_HARDWARE_SLIM,
    PS3SCALE_HARDWARE_SUPER_SLIM,
    PS3SCALE_HARDWARE_TOOL
} ps3scale_hardware_family_t;

typedef enum ps3scale_console_type {
    PS3SCALE_CONSOLE_UNKNOWN = 0,
    PS3SCALE_CONSOLE_CEX = 1,
    PS3SCALE_CONSOLE_DEX = 2,
    PS3SCALE_CONSOLE_DEH = 3
} ps3scale_console_type_t;

typedef struct ps3scale_hardware_info {
    char firmware_version[8];
    char platform_id[9];
    char model_series[48];
    uint8_t platform_id_code;
    uint32_t build_number;
    ps3scale_hardware_family_t family;
    ps3scale_console_type_t console_type;
} ps3scale_hardware_info_t;

/*
 * Parse the 0x18-byte sys_sm_get_system_info result (syscall 387).
 * This exposes firmware/platform revision only; it never reads IDPS or PSID.
 */
int ps3scale_hardware_info_parse(const uint8_t *raw, size_t raw_size,
                                 ps3scale_hardware_info_t *out);
const char *ps3scale_hardware_family_name(ps3scale_hardware_family_t family);
const char *ps3scale_console_type_name(ps3scale_console_type_t type);

/* Read-only GameOS query. Returns non-zero when the CFW denies syscall 387. */
int ps3scale_hardware_detect(ps3scale_hardware_info_t *out);

#endif
