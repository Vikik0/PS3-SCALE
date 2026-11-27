#include "ps3scale/ps2_config_path.h"

#include "ps3scale/game.h"

#include <stdio.h>
#include <string.h>

static int ascii_equal_ci(char a, char b)
{
    if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
    if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
    return a == b;
}

static int ends_with_ci(const char *value, const char *suffix)
{
    size_t value_length;
    size_t suffix_length;
    size_t i;

    if (!value || !suffix)
        return 0;
    value_length = strlen(value);
    suffix_length = strlen(suffix);
    if (suffix_length > value_length)
        return 0;

    value += value_length - suffix_length;
    for (i = 0; i < suffix_length; ++i) {
        if (!ascii_equal_ci(value[i], suffix[i]))
            return 0;
    }
    return 1;
}

static int has_dot_segment(const char *path)
{
    const char *cursor = path;

    while (*cursor) {
        const char *start;
        size_t length;

        if (*cursor == '/') {
            ++cursor;
            continue;
        }
        start = cursor;
        while (*cursor && *cursor != '/')
            ++cursor;
        length = (size_t)(cursor - start);
        if ((length == 1u && start[0] == '.') ||
            (length == 2u && start[0] == '.' && start[1] == '.'))
            return 1;
    }
    return 0;
}

static int supported_ps2_iso_path(const char *path, int *is_usb)
{
    static const char hdd_root[] = "/dev_hdd0/PS2ISO/";
    char usb_root[32];
    unsigned int index;
    size_t i;
    int usb = 0;

    if (!path || !path[0] || strlen(path) >= PS3SCALE_PATH_MAX)
        return 0;
    if (strncmp(path, hdd_root, sizeof(hdd_root) - 1u) == 0) {
        usb = 0;
    } else {
        for (index = 0; index <= 7u; ++index) {
            int written = snprintf(usb_root, sizeof(usb_root),
                                   "/dev_usb%03u/PS2ISO/", index);
            if (written > 0 && (size_t)written < sizeof(usb_root) &&
                strncmp(path, usb_root, (size_t)written) == 0) {
                usb = 1;
                break;
            }
        }
        if (index > 7u)
            return 0;
    }

    if (strstr(path, "//") || has_dot_segment(path))
        return 0;
    for (i = 0; path[i]; ++i) {
        unsigned char ch = (unsigned char)path[i];
        if (ch < 0x20u || ch == 0x7fu || ch == '\\')
            return 0;
    }

    {
        const char *basename = strrchr(path, '/');
        if (!basename || !basename[1] || basename[1] == '.')
            return 0;
    }

    if (!ends_with_ci(path, ".iso") &&
        !ends_with_ci(path, ".bin") &&
        !ends_with_ci(path, ".img") &&
        !ends_with_ci(path, ".mdf"))
        return 0;

    if (is_usb)
        *is_usb = usb;
    return 1;
}

int ps3scale_ps2_iso_config_path(const char *iso_path,
                                 char *output, size_t output_size,
                                 int *uses_hdd_cache)
{
    const char *basename;
    int is_usb = 0;
    int written;

    if (uses_hdd_cache)
        *uses_hdd_cache = 0;
    if (!output || output_size == 0)
        return -1;
    output[0] = '\0';

    if (!supported_ps2_iso_path(iso_path, &is_usb))
        return -1;

    if (!is_usb) {
        written = snprintf(output, output_size, "%s.CONFIG", iso_path);
    } else {
        basename = strrchr(iso_path, '/');
        if (!basename || !basename[1])
            return -1;
        written = snprintf(output, output_size, "%s/%s.CONFIG",
                           PS3SCALE_PS2_HDD_CACHE_DIR, basename + 1);
    }

    if (written < 0 || (size_t)written >= output_size) {
        output[0] = '\0';
        return -1;
    }

    if (uses_hdd_cache)
        *uses_hdd_cache = is_usb;
    return 0;
}
