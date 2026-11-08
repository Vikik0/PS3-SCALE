#include "ps3scale/profile_apply.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PS3SCALE_WEBMAN_SCRIPT_DIR "/dev_hdd0/tmp/wm_ingame"
#define PS3SCALE_SCRIPT_HEADER "# PS3 SCALE AUTO PROFILE V1"

static int make_script_path(char *path, size_t capacity, const char *title_id)
{
    if (!path || !capacity || !title_id || strlen(title_id) != 9)
        return -1;
    if (snprintf(path, capacity, PS3SCALE_WEBMAN_SCRIPT_DIR "/%s.bat",
                 title_id) >= (int)capacity)
        return -1;
    return 0;
}

static int read_script_header(const char *path, char *header, size_t capacity)
{
    FILE *file;
    size_t length;

    if (!path || !header || capacity < 2)
        return -1;
    header[0] = '\0';
    file = fopen(path, "rb");
    if (!file)
        return 0;
    if (!fgets(header, (int)capacity, file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    length = strlen(header);
    while (length && (header[length - 1] == '\n' ||
                      header[length - 1] == '\r'))
        header[--length] = '\0';
    return 1;
}

int ps3scale_profile_script_state(const char *title_id, const char *app_ver)
{
    char path[256];
    char header[160];
    char expected_id[32];
    char expected_version[24];
    int exists;

    if (make_script_path(path, sizeof(path), title_id) != 0)
        return -1;

    exists = read_script_header(path, header, sizeof(header));
    if (exists == 0)
        return 0;
    if (exists < 0 || strncmp(header, PS3SCALE_SCRIPT_HEADER,
                              strlen(PS3SCALE_SCRIPT_HEADER)) != 0)
        return -1;

    snprintf(expected_id, sizeof(expected_id), "TITLE_ID=%s", title_id);
    snprintf(expected_version, sizeof(expected_version), "APP_VER=%s",
             app_ver ? app_ver : "");
    if (strstr(header, expected_id) && strstr(header, expected_version))
        return 1;
    return -2; /* PS3 SCALE-owned script, but for a different update. */
}

static int valid_hex_string(const char *value, size_t max_length, int even)
{
    size_t i, length;

    if (!value || !value[0])
        return 0;
    length = strlen(value);
    if (length > max_length || (even && (length & 1u)))
        return 0;
    for (i = 0; i < length; i++) {
        if (!isxdigit((unsigned char)value[i]))
            return 0;
    }
    return 1;
}

static int parse_single_patch(const ps3scale_profile_t *profile,
                              unsigned long *address,
                              char *expected, size_t expected_capacity,
                              char *replacement, size_t replacement_capacity)
{
    char copy[PS3SCALE_PROFILE_PATCHES_MAX];
    char *first, *second, *third, *end = NULL;
    unsigned long parsed;

    if (!profile || profile->patch_count != 1 ||
        strlen(profile->patches) >= sizeof(copy))
        return -1;

    memcpy(copy, profile->patches, strlen(profile->patches) + 1);
    first = copy;
    second = strchr(first, ',');
    if (!second)
        return -1;
    *second++ = '\0';
    third = strchr(second, ',');
    if (!third)
        return -1;
    *third++ = '\0';
    if (strchr(third, ',') || strchr(third, ';'))
        return -1;

    if (!valid_hex_string(first, 8, 0) ||
        !valid_hex_string(second, 32, 1) ||
        !valid_hex_string(third, 32, 1) ||
        strlen(second) != strlen(third) ||
        strlen(second) + 1 > expected_capacity ||
        strlen(third) + 1 > replacement_capacity)
        return -1;

    parsed = strtoul(first, &end, 16);
    if (!end || *end || parsed == 0 || parsed >= 0xFFFFFFFEul)
        return -1;

    *address = parsed;
    strcpy(expected, second);
    strcpy(replacement, third);
    return 0;
}

int ps3scale_profile_script_install(const ps3scale_profile_t *profile)
{
    char path[256];
    char temporary_path[272];
    char header[160];
    char expected[34];
    char replacement[34];
    unsigned long address;
    FILE *file;
    int state;
    int written;

    if (!profile || strcmp(profile->trust, "B_COMMUNITY") != 0)
        return -1;
    if (profile->patch_count != 1)
        return -3; /* Multi-write profiles are not auto-applied in this build. */
    if (parse_single_patch(profile, &address, expected, sizeof(expected),
                           replacement, sizeof(replacement)) != 0)
        return -4;
    if (make_script_path(path, sizeof(path), profile->title_id) != 0)
        return -4;

    state = ps3scale_profile_script_state(profile->title_id, profile->app_ver);
    if (state == 1)
        return 1;
    if (state == -1)
        return -2; /* Never overwrite a script that belongs to the user. */

    if (mkdir("/dev_hdd0/tmp", 0777) != 0 && errno != EEXIST)
        return -5;
    if (mkdir(PS3SCALE_WEBMAN_SCRIPT_DIR, 0777) != 0 && errno != EEXIST)
        return -5;

    if (snprintf(temporary_path, sizeof(temporary_path), "%s.part", path) >=
        (int)sizeof(temporary_path))
        return -4;

    file = fopen(temporary_path, "wb");
    if (!file)
        return -5;

    /*
     * webMAN MOD runs /dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat when that
     * title starts. /patch.ps3 performs an exact byte search in a one-byte
     * address window before writing; a mismatched update is therefore
     * skipped rather than blindly overwritten.
     */
    written = fprintf(file,
        PS3SCALE_SCRIPT_HEADER " TITLE_ID=%s APP_VER=%s\n"
        "# Source: %s\n"
        "if titleid %s\n"
        "if ingame\n"
        "/patch.ps3?addr=%08lX&stop=%08lX&step=1&find=%s&rep=1&val=%s\n"
        "beep1\n"
        "popup PS3 SCALE profile check complete. Only exact byte matches are changed.\n"
        "end if\n"
        "end if\n",
        profile->title_id, profile->app_ver, profile->source_url,
        profile->title_id, address - 1, address + 1,
        expected, replacement);
    if (written < 0 || fclose(file) != 0) {
        unlink(temporary_path);
        return -5;
    }

    if (rename(temporary_path, path) != 0) {
        unlink(temporary_path);
        return -5;
    }
    return 0;
}

int ps3scale_profile_script_remove(const char *title_id)
{
    char path[256];
    char header[160];
    int exists;

    if (make_script_path(path, sizeof(path), title_id) != 0)
        return -2;
    exists = read_script_header(path, header, sizeof(header));
    if (exists == 0)
        return -1;
    if (exists < 0 || strncmp(header, PS3SCALE_SCRIPT_HEADER,
                              strlen(PS3SCALE_SCRIPT_HEADER)) != 0)
        return -2;
    return unlink(path) == 0 ? 0 : -2;
}
