#include "ps3scale/profile_apply.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PS3SCALE_WEBMAN_TMP_DIR
#define PS3SCALE_WEBMAN_TMP_DIR "/dev_hdd0/tmp"
#endif

#ifndef PS3SCALE_WEBMAN_SCRIPT_DIR
#define PS3SCALE_WEBMAN_SCRIPT_DIR "/dev_hdd0/tmp/wm_ingame"
#endif

#define PS3SCALE_SCRIPT_HEADER_V1 "# PS3 SCALE AUTO PROFILE V1"
#define PS3SCALE_SCRIPT_HEADER_V2 "# PS3 SCALE AUTO PROFILE V2"
#define PS3SCALE_PROFILE_SCRIPT_MAX 1024u
#define PS3SCALE_PROFILE_SEARCH_WINDOW 0x20ul
#define PS3SCALE_PROFILE_SEARCH_STEP 64ul

static int valid_title_id(const char *value)
{
    size_t i;

    if (!value || strlen(value) != 9)
        return 0;
    for (i = 0; i < 9; i++) {
        if (!((value[i] >= 'A' && value[i] <= 'Z') ||
              (value[i] >= '0' && value[i] <= '9')))
            return 0;
    }
    return 1;
}

static int valid_app_version(const char *value)
{
    return value && strlen(value) == 5 &&
           isdigit((unsigned char)value[0]) &&
           isdigit((unsigned char)value[1]) &&
           value[2] == '.' &&
           isdigit((unsigned char)value[3]) &&
           isdigit((unsigned char)value[4]);
}

static int valid_source_url(const char *value)
{
    size_t i, length;

    if (!value || !memchr(value, '\0', PS3SCALE_PROFILE_SOURCE_MAX))
        return 0;
    length = strlen(value);
    if (length < 8 || length >= PS3SCALE_PROFILE_SOURCE_MAX ||
        strncmp(value, "https://", 8) != 0)
        return 0;

    /* The URL is not executed; reject control characters before accepting it. */
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (ch <= 0x20u || ch >= 0x7fu)
            return 0;
    }
    return 1;
}

static int make_script_path(char *path, size_t capacity, const char *title_id)
{
    if (!path || !capacity || !valid_title_id(title_id))
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
        return errno == ENOENT ? 0 : -1;
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

/* Return 0 when absent, 1 when read, and -1/-2 for read/size errors. */
static int read_script_contents(const char *path, char *contents,
                                size_t capacity)
{
    FILE *file;
    size_t used;
    int extra;

    if (!path || !contents || capacity < 2)
        return -1;
    contents[0] = '\0';
    file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT ? 0 : -1;

    used = fread(contents, 1, capacity - 1, file);
    if (ferror(file)) {
        fclose(file);
        contents[0] = '\0';
        return -1;
    }
    if (used == capacity - 1) {
        extra = fgetc(file);
        if (extra != EOF) {
            fclose(file);
            contents[0] = '\0';
            return -2;
        }
        if (ferror(file)) {
            fclose(file);
            contents[0] = '\0';
            return -1;
        }
    }
    contents[used] = '\0';
    if (fclose(file) != 0) {
        contents[0] = '\0';
        return -1;
    }
    return 1;
}

static int header_has_marker(const char *header, const char *marker)
{
    size_t length;

    if (!header || !marker)
        return 0;
    length = strlen(marker);
    return strncmp(header, marker, length) == 0 &&
           (header[length] == '\0' || header[length] == ' ');
}

static int header_is_owned(const char *header)
{
    return header_has_marker(header, PS3SCALE_SCRIPT_HEADER_V1) ||
           header_has_marker(header, PS3SCALE_SCRIPT_HEADER_V2);
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

static int parse_single_patch_string(const char *patches,
                                     unsigned long *address,
                                     char *expected, size_t expected_capacity,
                                     char *replacement,
                                     size_t replacement_capacity)
{
    char copy[PS3SCALE_PROFILE_PATCHES_MAX];
    char *first, *second, *third, *end = NULL;
    unsigned long parsed;

    if (!patches || !patches[0] ||
        strlen(patches) >= sizeof(copy))
        return -1;

    memcpy(copy, patches, strlen(patches) + 1);
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
        strcmp(second, third) == 0 ||
        strlen(second) + 1 > expected_capacity ||
        strlen(third) + 1 > replacement_capacity)
        return -1;

    parsed = strtoul(first, &end, 16);
    if (!end || *end || parsed == 0 || parsed > 0xFFFFFFDFul)
        return -1;

    *address = parsed;
    strcpy(expected, second);
    strcpy(replacement, third);
    return 0;
}

/*
 * webMAN rounds its safe upper search bound down to a 16-byte boundary.
 * With signatures limited to 1-16 bytes, stop=address+0x20 and step=64
 * leave exactly one candidate start address: the requested address.
 */
static int format_profile_script(char *output, size_t capacity,
                                 const char *title_id, const char *app_ver,
                                 const char *patches)
{
    char path[256];
    char header[384];
    char expected[34];
    char replacement[34];
    unsigned long address;
    int written;

    if (!output || !capacity ||
        !valid_title_id(title_id) || !valid_app_version(app_ver) ||
        parse_single_patch_string(patches, &address, expected,
                                  sizeof(expected), replacement,
                                  sizeof(replacement)) != 0 ||
        make_script_path(path, sizeof(path), title_id) != 0)
        return -1;

    if (snprintf(header, sizeof(header),
            PS3SCALE_SCRIPT_HEADER_V2
            " TITLE_ID=%s APP_VER=%s PATCHES=%s",
            title_id, app_ver, patches) >= (int)sizeof(header))
        return -1;

    /*
     * The source URL stays in the catalogue/UI, not in the executable script.
     * This makes the complete generated script deterministic so its full body
     * can be compared before PS3 SCALE allows the game to launch.
     */
    written = snprintf(output, capacity,
        "%s\n"
        "# One launch only. No success notification is shown because this "
        "batch command cannot verify the post-write memory value.\n"
        "if titleid %s\n"
        "if ingame\n"
        "/patch.ps3?addr=%08lX&stop=%08lX&step=%lu&find=%s&rep=1&val=%s\n"
        "end if\n"
        "end if\n"
        "del %s\n\n",
        header, title_id, address, address + PS3SCALE_PROFILE_SEARCH_WINDOW,
        PS3SCALE_PROFILE_SEARCH_STEP, expected, replacement, path);
    return written < 0 || (size_t)written >= capacity ? -1 : 0;
}

/*
 * Returns 1 only when the entire installed script exactly matches the
 * deterministic PS3 SCALE V2 script for this Title ID, APP_VER and patch.
 * A stale, legacy or modified PS3 SCALE script returns -2.
 */
int ps3scale_profile_script_state(const char *title_id, const char *app_ver,
                                  const char *patches)
{
    char path[256];
    char header[384];
    char expected_script[PS3SCALE_PROFILE_SCRIPT_MAX];
    char actual_script[PS3SCALE_PROFILE_SCRIPT_MAX];
    int exists;
    int content_state;

    if (make_script_path(path, sizeof(path), title_id) != 0)
        return -1;

    exists = read_script_header(path, header, sizeof(header));
    if (exists == 0)
        return 0;
    if (exists < 0 || !header_is_owned(header))
        return -1;

    if (strncmp(header, PS3SCALE_SCRIPT_HEADER_V1,
                strlen(PS3SCALE_SCRIPT_HEADER_V1)) == 0)
        return -2;

    if (format_profile_script(expected_script, sizeof(expected_script),
                              title_id, app_ver, patches) != 0)
        return -2;

    content_state = read_script_contents(path, actual_script,
                                         sizeof(actual_script));
    if (content_state != 1)
        return -2;

    return strcmp(actual_script, expected_script) == 0 ? 1 : -2;
}

int ps3scale_profile_script_install(const ps3scale_profile_t *profile)
{
    char path[256];
    char temporary_path[272];
    char script[PS3SCALE_PROFILE_SCRIPT_MAX];
    FILE *file;
    size_t script_length;
    int state;

    if (!profile || strcmp(profile->trust, "B_COMMUNITY") != 0 ||
        !valid_title_id(profile->title_id) ||
        !valid_app_version(profile->app_ver) ||
        !valid_source_url(profile->source_url))
        return -1;
    if (profile->patch_count != 1)
        return -3; /* Multi-write profiles remain manual-only in this build. */
    if (format_profile_script(script, sizeof(script), profile->title_id,
                              profile->app_ver, profile->patches) != 0 ||
        make_script_path(path, sizeof(path), profile->title_id) != 0)
        return -4;

    state = ps3scale_profile_script_state(profile->title_id, profile->app_ver,
                                          profile->patches);
    if (state == 1)
        return 1;
    if (state == -1)
        return -2; /* Never overwrite a script that belongs to the user. */

    if (mkdir(PS3SCALE_WEBMAN_TMP_DIR, 0777) != 0 && errno != EEXIST)
        return -5;
    if (mkdir(PS3SCALE_WEBMAN_SCRIPT_DIR, 0777) != 0 && errno != EEXIST)
        return -5;

    if (snprintf(temporary_path, sizeof(temporary_path), "%s.part", path) >=
        (int)sizeof(temporary_path))
        return -4;

    file = fopen(temporary_path, "wb");
    if (!file)
        return -5;

    script_length = strlen(script);
    if (fwrite(script, 1, script_length, file) != script_length) {
        (void)fclose(file);
        unlink(temporary_path);
        return -5;
    }
    if (fclose(file) != 0) {
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
    char header[384];
    int exists;

    if (make_script_path(path, sizeof(path), title_id) != 0)
        return -2;
    exists = read_script_header(path, header, sizeof(header));
    if (exists == 0)
        return -1;
    if (exists < 0 || !header_is_owned(header))
        return -2;
    return unlink(path) == 0 ? 0 : -2;
}
