#include "ps3scale/hardware_info.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_platform(uint8_t raw[24], const char *platform_id,
                         uint8_t platform_code)
{
    raw[7] = platform_code;
    strncpy((char *)&raw[8], platform_id, 8);
}

int main(void)
{
    uint8_t raw[24];
    ps3scale_hardware_info_t info;

    memset(raw, 0, sizeof(raw));
    raw[0] = 0x03;
    raw[1] = 0x05;
    raw[2] = 0x50;
    raw[16] = 0x00; raw[17] = 0x00; raw[18] = 0xb5; raw[19] = 0xed;
    set_platform(raw, "CokB10", 0x40);
    assert(ps3scale_hardware_info_parse(raw, sizeof(raw), &info) == 0);
    assert(strcmp(info.firmware_version, "3.55") == 0);
    assert(strcmp(info.platform_id, "CokB10") == 0);
    assert(info.platform_id_code == 0x40u);
    assert(info.build_number == 0xb5edu);
    assert(info.family == PS3SCALE_HARDWARE_FAT);
    assert(strstr(info.model_series, "CECHC") != NULL);

    memset(raw, 0, sizeof(raw));
    raw[0] = 0x04;
    raw[1] = 0x09;
    raw[2] = 0x20;
    set_platform(raw, "CokJ13", 0x80);
    assert(ps3scale_hardware_info_parse(raw, sizeof(raw), &info) == 0);
    assert(strcmp(info.firmware_version, "4.92") == 0);
    assert(info.family == PS3SCALE_HARDWARE_SLIM);
    assert(strstr(info.model_series, "CECH-25xx") != NULL);

    memset(raw, 0, sizeof(raw));
    raw[0] = 0x04;
    raw[1] = 0x09;
    raw[2] = 0x20;
    set_platform(raw, "CokM30", 0xb8);
    assert(ps3scale_hardware_info_parse(raw, sizeof(raw), &info) == 0);
    assert(info.family == PS3SCALE_HARDWARE_SUPER_SLIM);
    assert(strstr(info.model_series, "CECH-40xx") != NULL);

    memset(raw, 0, sizeof(raw));
    raw[0] = 0x04;
    raw[1] = 0x09;
    raw[2] = 0x20;
    set_platform(raw, "CokZZZ", 0xff);
    assert(ps3scale_hardware_info_parse(raw, sizeof(raw), &info) == 0);
    assert(info.family == PS3SCALE_HARDWARE_UNKNOWN);
    assert(strcmp(info.model_series, "Unknown PS3 hardware revision") == 0);

    assert(ps3scale_hardware_info_parse(raw, 23, &info) == -1);
    assert(ps3scale_hardware_info_parse(NULL, 24, &info) == -1);
    assert(ps3scale_hardware_info_parse(raw, 24, NULL) == -1);
    assert(strcmp(ps3scale_hardware_family_name(
                      PS3SCALE_HARDWARE_SUPER_SLIM), "SUPER SLIM") == 0);
    assert(strcmp(ps3scale_console_type_name(PS3SCALE_CONSOLE_CEX), "CEX") == 0);
    assert(strcmp(ps3scale_console_type_name(PS3SCALE_CONSOLE_DEX), "DEX") == 0);
    assert(strcmp(ps3scale_console_type_name(PS3SCALE_CONSOLE_DEH), "DEH") == 0);
    assert(strcmp(ps3scale_console_type_name(PS3SCALE_CONSOLE_UNKNOWN), "UNKNOWN") == 0);

    puts("PS3 SCALE hardware information tests: PASS");
    return 0;
}
