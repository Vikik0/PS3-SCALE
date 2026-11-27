#include "ps3scale/ps2_config_path.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_path(const char *iso, const char *expected, int expected_cache)
{
    char output[512];
    int uses_cache = -1;

    assert(ps3scale_ps2_iso_config_path(
        iso, output, sizeof(output), &uses_cache) == 0);
    assert(strcmp(output, expected) == 0);
    assert(uses_cache == expected_cache);
}

static void expect_invalid(const char *iso)
{
    char output[512];
    int uses_cache = -1;

    assert(ps3scale_ps2_iso_config_path(
        iso, output, sizeof(output), &uses_cache) != 0);
    assert(output[0] == '\0');
    assert(uses_cache == 0);
}

int main(void)
{
    char output[512];

    expect_path("/dev_hdd0/PS2ISO/Crash.iso",
                "/dev_hdd0/PS2ISO/Crash.iso.CONFIG", 0);
    expect_path("/dev_usb000/PS2ISO/Crash.iso",
                "/dev_hdd0/PS2ISO/Crash.iso.CONFIG", 1);
    expect_path("/dev_usb007/PS2ISO/Shadow of Rome [SLUS_206.27].MDF",
                "/dev_hdd0/PS2ISO/Shadow of Rome [SLUS_206.27].MDF.CONFIG", 1);
    expect_path("/dev_hdd0/PS2ISO/Gran Turismo.bin",
                "/dev_hdd0/PS2ISO/Gran Turismo.bin.CONFIG", 0);

    expect_invalid(NULL);
    expect_invalid("");
    expect_invalid("/dev_hdd0/PS3ISO/Crash.iso");
    expect_invalid("/dev_usb001/GAMES/Crash.iso");
    expect_invalid("/dev_usb001/PS2ISO/Crash.bin.enc");
    expect_invalid("/dev_usb001/PS2ISO/../Crash.iso");
    expect_invalid("/dev_usb001/PS2ISO//Crash.iso");
    expect_invalid("/dev_usb001/PS2ISO/./Crash.iso");
    expect_invalid("/dev_usb001/PS2ISO/");

    assert(ps3scale_ps2_iso_config_path(
        "/dev_usb000/PS2ISO/Crash.iso", output, 8, NULL) != 0);
    assert(output[0] == '\0');

    puts("PS3 SCALE PS2 CONFIG target-path tests: PASS");
    return 0;
}
