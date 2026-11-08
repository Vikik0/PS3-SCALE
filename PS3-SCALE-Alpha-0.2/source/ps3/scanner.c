#include "ps3scale/scanner.h"
#include "ps3scale/sfo.h"
#include "ps3scale/iso9660.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SFO_MAX_BYTES (1024u * 1024u)

static int path_join(char *out, size_t cap, const char *a, const char *b)
{
    int n;
    if (!out || !a || !b) return -1;
    n = snprintf(out, cap, "%s/%s", a, b);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

static int is_directory(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int ends_with_ci(const char *s, const char *ext)
{
    size_t a, b, i;
    if (!s || !ext) return 0;
    a = strlen(s);
    b = strlen(ext);
    if (a < b) return 0;
    s += a - b;
    for (i = 0; i < b; i++)
        if (tolower((unsigned char)s[i]) !=
            tolower((unsigned char)ext[i])) return 0;
    return 1;
}

static void basename_no_ext(char *dst, size_t cap, const char *path)
{
    const char *base = strrchr(path, '/');
    const char *dot;
    size_t n;
    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= cap) n = cap - 1;
    memcpy(dst, base, n);
    dst[n] = '\0';
}

static int read_sfo(const char *path, ps3scale_sfo_info_t *info)
{
    FILE *fp;
    long size;
    uint8_t *buf;
    int result;

    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    size = ftell(fp);
    if (size < 20 || (unsigned long)size > SFO_MAX_BYTES) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }

    buf = (uint8_t *)malloc((size_t)size);
    if (!buf) { fclose(fp); return -1; }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    result = ps3scale_sfo_parse(buf, (size_t)size, info);
    free(buf);
    return result;
}

static int category_is_ps3_disc(const char *cat)
{
    return cat && strcmp(cat, "DG") == 0;
}

static int category_is_ps3_hdd(const char *cat)
{
    return cat && strcmp(cat, "HG") == 0;
}

static int category_is_ps2_classic(const char *cat)
{
    return cat && strcmp(cat, "2P") == 0;
}

static int category_is_ps2_hdd(const char *cat)
{
    return cat && strcmp(cat, "2G") == 0;
}

static int add_sfo_game(const char *sfo_path, const char *game_path,
                        ps3scale_game_kind_t kind,
                        ps3scale_game_list_t *list,
                        int require_disc_category)
{
    ps3scale_sfo_info_t info;
    ps3scale_game_t game;

    if (read_sfo(sfo_path, &info) != 0) return 0;
    if (require_disc_category &&
        !category_is_ps3_disc(info.category)) return 0;

    memset(&game, 0, sizeof(game));
    game.kind = kind;
    strncpy(game.title, info.title[0] ? info.title : info.title_id,
            sizeof(game.title) - 1);
    strncpy(game.title_id, info.title_id, sizeof(game.title_id) - 1);
    strncpy(game.category, info.category, sizeof(game.category) - 1);
    strncpy(game.path, game_path, sizeof(game.path) - 1);
    return ps3scale_game_list_add(list, &game) > 0 ? 1 : 0;
}

static int scan_folder_games(const char *base, ps3scale_game_list_t *list)
{
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir(base);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char folder[PS3SCALE_PATH_MAX];
        char sfo[PS3SCALE_PATH_MAX];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        if (path_join(folder, sizeof(folder), base, entry->d_name) != 0)
            continue;
        if (stat(folder, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (path_join(sfo, sizeof(sfo), folder, "PS3_GAME/PARAM.SFO") == 0) {
            found += add_sfo_game(sfo, folder, PS3SCALE_GAME_PS3_FOLDER,
                                  list, 1);
        }
    }

    closedir(dir);
    return found;
}

static int scan_installed_titles(const char *base,
                                 ps3scale_game_list_t *list)
{
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir(base);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char folder[PS3SCALE_PATH_MAX];
        char sfo[PS3SCALE_PATH_MAX];
        ps3scale_sfo_info_t info;
        ps3scale_game_t game;
        ps3scale_game_kind_t kind;

        if (entry->d_name[0] == '.') continue;
        if (path_join(folder, sizeof(folder), base, entry->d_name) != 0)
            continue;
        if (!is_directory(folder)) continue;
        if (path_join(sfo, sizeof(sfo), folder, "PARAM.SFO") != 0)
            continue;
        if (read_sfo(sfo, &info) != 0) continue;

        if (category_is_ps3_hdd(info.category))
            kind = PS3SCALE_GAME_PS3_HDD;
        else if (category_is_ps2_classic(info.category))
            kind = PS3SCALE_GAME_PS2_CLASSIC;
        else if (category_is_ps2_hdd(info.category))
            kind = PS3SCALE_GAME_PS2_HDD;
        else
            continue; /* Do not mistake saves, DLC, apps, or game data for games. */

        memset(&game, 0, sizeof(game));
        game.kind = kind;
        strncpy(game.title, info.title[0] ? info.title : info.title_id,
                sizeof(game.title) - 1);
        strncpy(game.title_id, info.title_id, sizeof(game.title_id) - 1);
        strncpy(game.category, info.category, sizeof(game.category) - 1);
        strncpy(game.path, folder, sizeof(game.path) - 1);
        if (ps3scale_game_list_add(list, &game) > 0) found++;
    }

    closedir(dir);
    return found;
}

static int scan_iso_files(const char *base, ps3scale_game_kind_t kind,
                          ps3scale_game_list_t *list)
{
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir(base);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char path[PS3SCALE_PATH_MAX];
        struct stat st;
        ps3scale_game_t game;
        int accepted = 0;

        if (entry->d_name[0] == '.') continue;
        if (kind == PS3SCALE_GAME_PS3_ISO)
            accepted = ends_with_ci(entry->d_name, ".iso");
        else
            accepted = ends_with_ci(entry->d_name, ".iso") ||
                       ends_with_ci(entry->d_name, ".bin");
        if (!accepted) continue;

        if (path_join(path, sizeof(path), base, entry->d_name) != 0)
            continue;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        memset(&game, 0, sizeof(game));
        game.kind = kind;
        basename_no_ext(game.title, sizeof(game.title), entry->d_name);
        strncpy(game.path, path, sizeof(game.path) - 1);

        if (kind == PS3SCALE_GAME_PS3_ISO) {
            (void)ps3scale_title_id_from_filename(entry->d_name,
                                                  game.title_id,
                                                  sizeof(game.title_id));
        } else {
            if (ps3scale_ps2_iso_read_serial(path, game.disc_id,
                                             sizeof(game.disc_id)) != 0) {
                (void)ps3scale_ps2_serial_from_filename(entry->d_name,
                                                        game.disc_id,
                                                        sizeof(game.disc_id));
            }
        }

        if (ps3scale_game_list_add(list, &game) > 0) found++;
    }

    closedir(dir);
    return found;
}


static int scan_layout(const char *base, ps3scale_game_list_t *list)
{
    char path[PS3SCALE_PATH_MAX];
    int found = 0;

    if (path_join(path, sizeof(path), base, "GAMES") == 0)
        found += scan_folder_games(path, list);
    if (path_join(path, sizeof(path), base, "GAMEZ") == 0)
        found += scan_folder_games(path, list);
    if (path_join(path, sizeof(path), base, "game") == 0)
        found += scan_installed_titles(path, list);
    if (path_join(path, sizeof(path), base, "PS3ISO") == 0)
        found += scan_iso_files(path, PS3SCALE_GAME_PS3_ISO, list);
    if (path_join(path, sizeof(path), base, "PS2ISO") == 0)
        found += scan_iso_files(path, PS3SCALE_GAME_PS2_ISO, list);

    return found;
}

int ps3scale_scan_device(const char *device_root, ps3scale_game_list_t *list)
{
    char path[PS3SCALE_PATH_MAX];
    char sfo[PS3SCALE_PATH_MAX];
    int found = 0;

    if (!device_root || !list) return -1;

    /* A mounted PS3 Blu-ray is not inside a GAMES subfolder. */
    if (path_join(sfo, sizeof(sfo), device_root,
                  "PS3_GAME/PARAM.SFO") == 0) {
        found += add_sfo_game(sfo, device_root, PS3SCALE_GAME_PS3_FOLDER,
                              list, 1);
    }

    found += scan_layout(device_root, list);

    /* Support common external-drive layout: /dev_usb000/PS3/PS3ISO, etc. */
    if (path_join(path, sizeof(path), device_root, "PS3") == 0 &&
        is_directory(path)) {
        found += scan_layout(path, list);
    }

    return found;
}

int ps3scale_scan_all(ps3scale_game_list_t *list)
{
    char root[32];
    int found = 0;
    int i;

    if (!list) return -1;
    ps3scale_game_list_init(list);

    found += ps3scale_scan_device("/dev_hdd0", list);
    found += ps3scale_scan_device("/dev_bdvd", list);

    for (i = 0; i < 8; i++) {
        snprintf(root, sizeof(root), "/dev_usb%03d", i);
        found += ps3scale_scan_device(root, list);
    }

    return found;
}
