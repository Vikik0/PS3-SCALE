#include "disc_browser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int has_cue_extension(const char *name)
{
    size_t length;
    if (!name) return 0;
    length = strlen(name);
    if (length < 4) return 0;
    return name[length - 4] == '.' &&
           tolower((unsigned char)name[length - 3]) == 'c' &&
           tolower((unsigned char)name[length - 2]) == 'u' &&
           tolower((unsigned char)name[length - 1]) == 'e';
}

static int already_added(const CBGameDisc *items, size_t count, const char *path)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(items[i].path, path) == 0) return 1;
    }
    return 0;
}

int cb_disc_scan_roots(CBGameDisc *items, size_t capacity, size_t *count,
                       const char *const *roots, size_t root_count)
{
    size_t root_index;
    if (!items || capacity == 0 || !count || (!roots && root_count > 0)) return -1;
    *count = 0;

    for (root_index = 0; root_index < root_count && *count < capacity; root_index++) {
        DIR *directory;
        struct dirent *entry;
        size_t root_length;
        const char *separator;

        if (!roots[root_index] || !roots[root_index][0]) continue;
        directory = opendir(roots[root_index]);
        if (!directory) continue;

        root_length = strlen(roots[root_index]);
        separator = roots[root_index][root_length - 1] == '/' ? "" : "/";

        while ((entry = readdir(directory)) != NULL && *count < capacity) {
            char full_path[CB_DISC_PATH_MAX];
            char display_name[CB_DISC_NAME_MAX];
            struct stat info;
            size_t name_length;
            int path_length;

            if (!has_cue_extension(entry->d_name)) continue;
            path_length = snprintf(full_path, sizeof(full_path), "%s%s%s",
                                   roots[root_index], separator, entry->d_name);
            if (path_length < 0 || (size_t)path_length >= sizeof(full_path)) continue;
            if (already_added(items, *count, full_path)) continue;
            if (stat(full_path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) continue;

            name_length = strlen(entry->d_name);
            if (name_length >= 4) name_length -= 4;
            if (name_length >= sizeof(display_name)) name_length = sizeof(display_name) - 1;
            memcpy(display_name, entry->d_name, name_length);
            display_name[name_length] = '\0';

            snprintf(items[*count].path, sizeof(items[*count].path), "%s", full_path);
            snprintf(items[*count].name, sizeof(items[*count].name), "%s", display_name);
            (*count)++;
        }

        closedir(directory);
    }

    return 0;
}

int cb_disc_scan(CBGameDisc *items, size_t capacity, size_t *count)
{
    static const char *const roots[] = {
        "/dev_hdd0/game/CBON00001/USRDIR/PSXISO",
        "/dev_hdd0/PSXISO",
        "/dev_hdd0/PSXGAMES",
        "/dev_usb000/PSXISO",
        "/dev_usb001/PSXISO",
        "/dev_usb002/PSXISO",
        "/dev_usb003/PSXISO"
    };
    return cb_disc_scan_roots(items, capacity, count, roots,
                              sizeof(roots) / sizeof(roots[0]));
}
