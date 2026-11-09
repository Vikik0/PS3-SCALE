#include "ps3scale/webman_command.h"

#include <stdio.h>
#include <string.h>

static int valid_title_id(const char *value)
{
    size_t i;

    if (!value)
        return 0;
    for (i = 0; i < 9; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')))
            return 0;
    }
    return value[9] == '\0';
}

static int path_has_dot_segment(const char *path)
{
    const char *cursor = path;

    while (*cursor) {
        const char *start;
        size_t length;

        if (*cursor == '/') {
            cursor++;
            continue;
        }
        start = cursor;
        while (*cursor && *cursor != '/')
            cursor++;
        length = (size_t)(cursor - start);
        if ((length == 1 && start[0] == '.') ||
            (length == 2 && start[0] == '.' && start[1] == '.'))
            return 1;
    }
    return 0;
}

static int allowed_game_path(const char *path)
{
    static const char *roots[] = {
        "/dev_hdd0/", "/dev_bdvd/", "/dev_usb000/", "/dev_usb001/",
        "/dev_usb002/", "/dev_usb003/", "/dev_usb004/", "/dev_usb005/",
        "/dev_usb006/", "/dev_usb007/"
    };
    size_t i, length;

    if (!path)
        return 0;
    if (strcmp(path, "/dev_bdvd") == 0)
        return 1;

    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        length = strlen(roots[i]);
        if (strncmp(path, roots[i], length) == 0 && path[length] != '\0')
            break;
    }
    if (i == sizeof(roots) / sizeof(roots[0]))
        return 0;
    if (strstr(path, "//") || path_has_dot_segment(path))
        return 0;

    for (i = 0; path[i]; i++) {
        unsigned char ch = (unsigned char)path[i];
        if (ch < 0x20u || ch == 0x7fu || ch == '\\')
            return 0;
    }
    return 1;
}

static int encode_game_path(const char *source, char *destination,
                            size_t capacity)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    size_t i;

    if (!source || !destination || capacity == 0 ||
        !allowed_game_path(source))
        return -1;

    for (i = 0; source[i]; i++) {
        unsigned char ch = (unsigned char)source[i];
        int safe = (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch >= '0' && ch <= '9') ||
                   ch == '-' || ch == '_' || ch == '.' ||
                   ch == '~' || ch == '/';
        size_t needed = safe ? 1u : 3u;

        if (used + needed >= capacity)
            return -1;
        if (safe) {
            destination[used++] = (char)ch;
        } else {
            destination[used++] = '%';
            destination[used++] = hex[ch >> 4];
            destination[used++] = hex[ch & 0x0f];
        }
    }
    destination[used] = '\0';
    return 0;
}

static int path_ends_with_ci(const char *path, const char *suffix)
{
    size_t path_length, suffix_length, i;
    if (!path || !suffix)
        return 0;
    path_length = strlen(path);
    suffix_length = strlen(suffix);
    if (suffix_length > path_length)
        return 0;
    path += path_length - suffix_length;
    for (i = 0; i < suffix_length; i++) {
        char a = path[i];
        char b = suffix[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
        if (a != b)
            return 0;
    }
    return 1;
}

static int valid_ps2_iso_path(const char *path)
{
    if (!allowed_game_path(path) || !strstr(path, "/PS2ISO/"))
        return 0;
    return path_ends_with_ci(path, ".ISO") ||
           path_ends_with_ci(path, ".BIN");
}

int ps3scale_webman_build_start_path(const ps3scale_game_t *game,
                                     char *output, size_t output_size)
{
    char encoded_path[PS3SCALE_PATH_MAX * 3 + 1];
    int written;

    if (!game || !output || output_size == 0) {
        if (output && output_size > 0)
            output[0] = '\0';
        return -1;
    }
    output[0] = '\0';

    if (game->kind == PS3SCALE_GAME_PS2_ISO) {
        if (!valid_ps2_iso_path(game->path) ||
            encode_game_path(game->path, encoded_path,
                             sizeof(encoded_path)) != 0)
            return -1;
        written = snprintf(output, output_size, "/play.ps3%s", encoded_path);
    } else if (!valid_title_id(game->title_id)) {
        return -1;
    } else if (game->kind == PS3SCALE_GAME_PS3_HDD) {
        written = snprintf(output, output_size, "/play.ps3?%s",
                           game->title_id);
    } else if (game->kind == PS3SCALE_GAME_PS3_FOLDER ||
               game->kind == PS3SCALE_GAME_PS3_ISO) {
        if (strcmp(game->path, "/dev_bdvd") == 0) {
            written = snprintf(output, output_size, "/play.ps3");
        } else {
            if (encode_game_path(game->path, encoded_path,
                                 sizeof(encoded_path)) != 0)
                return -1;
            written = snprintf(output, output_size, "/play.ps3%s",
                               encoded_path);
        }
    } else {
        return -1;
    }

    if (written < 0 || (size_t)written >= output_size) {
        output[0] = '\0';
        return -1;
    }
    return 0;
}
