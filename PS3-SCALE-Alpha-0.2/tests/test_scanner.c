#define _XOPEN_SOURCE 700
#include "ps3scale/scanner.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void put16(unsigned char *p, unsigned short v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)(v >> 8);
}
static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static size_t add_entry(unsigned char *buf, size_t idx,
                        unsigned short keyoff, const char *value,
                        unsigned int valueoff)
{
    unsigned char *e = buf + 20 + idx * 16;
    size_t len = strlen(value) + 1;
    put16(e, keyoff);
    put16(e + 2, 0x0204);
    put32(e + 4, (unsigned int)len);
    put32(e + 8, (unsigned int)len);
    put32(e + 12, valueoff);
    return len;
}

static size_t make_sfo(unsigned char *buf, const char *title,
                       const char *title_id, const char *category)
{
    const size_t entry_bytes = 3 * 16;
    const size_t keyoff = 20 + entry_bytes;
    const char keys[] = "TITLE\0TITLE_ID\0CATEGORY\0";
    const size_t dataoff = keyoff + sizeof(keys);
    size_t p = dataoff, n;

    memset(buf, 0, 512);
    buf[0] = 0; buf[1] = 'P'; buf[2] = 'S'; buf[3] = 'F';
    put32(buf + 4, 0x00000101);
    put32(buf + 8, (unsigned int)keyoff);
    put32(buf + 12, (unsigned int)dataoff);
    put32(buf + 16, 3);
    memcpy(buf + keyoff, keys, sizeof(keys));

    n = add_entry(buf, 0, 0, title, (unsigned int)(p - dataoff));
    memcpy(buf + p, title, n); p += n;
    n = add_entry(buf, 1, 6, title_id, (unsigned int)(p - dataoff));
    memcpy(buf + p, title_id, n); p += n;
    n = add_entry(buf, 2, 15, category, (unsigned int)(p - dataoff));
    memcpy(buf + p, category, n); p += n;
    return p;
}

static void make_dir(const char *path)
{
    if (mkdir(path, 0777) != 0) {
        struct stat st;
        assert(stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    }
}

static void write_sfo(const char *path, const char *title,
                      const char *id, const char *category)
{
    unsigned char buf[512];
    size_t size = make_sfo(buf, title, id, category);
    FILE *fp = fopen(path, "wb");
    assert(fp);
    assert(fwrite(buf, 1, size, fp) == size);
    fclose(fp);
}

static void write_file(const char *path)
{
    FILE *fp = fopen(path, "wb");
    assert(fp);
    fputs("fixture", fp);
    fclose(fp);
}

int main(void)
{
    char temp[] = "/tmp/ps3scale-scan-XXXXXX";
    char path[1024];
    char *root = mkdtemp(temp);
    ps3scale_game_list_t list;
    int found;

    assert(root != NULL);

    snprintf(path, sizeof(path), "%s/GAMES", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/GAMES/DemoGame", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/GAMES/DemoGame/PS3_GAME", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/GAMES/DemoGame/PS3_GAME/PARAM.SFO", root);
    write_sfo(path, "Demo PS3 Game", "BLUS12345", "DG");
    snprintf(path, sizeof(path), "%s/GAMES/DemoGame/PS3_GAME/ICON0.PNG", root);
    write_file(path);

    snprintf(path, sizeof(path), "%s/game", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/game/PS2U10000", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/game/PS2U10000/PARAM.SFO", root);
    write_sfo(path, "Demo PS2 Classic", "PS2U10000", "2P");
    snprintf(path, sizeof(path), "%s/game/PS2U10000/ICON0.PNG", root);
    write_file(path);

    snprintf(path, sizeof(path), "%s/PS2ISO", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/PS2ISO/God of War II.iso", root);
    write_file(path);

    snprintf(path, sizeof(path), "%s/PS3ISO", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/PS3ISO/Gran Turismo 6 [BCES01893].iso", root);
    write_file(path);

    /* Mounted Blu-ray root: /dev_bdvd/PS3_GAME/PARAM.SFO. */
    snprintf(path, sizeof(path), "%s/PS3_GAME", root); make_dir(path);
    snprintf(path, sizeof(path), "%s/PS3_GAME/PARAM.SFO", root);
    write_sfo(path, "Mounted Disc Game", "BLES12345", "DG");

    ps3scale_game_list_init(&list);
    found = ps3scale_scan_device(root, &list);

    assert(found == 5);
    assert(list.count == 5);
    assert(list.items[0].kind == PS3SCALE_GAME_PS3_FOLDER);
    assert(list.items[1].kind == PS3SCALE_GAME_PS2_CLASSIC ||
           list.items[2].kind == PS3SCALE_GAME_PS2_CLASSIC);
    assert(list.items[0].title_id[0] != '\0');
    assert(list.items[0].cover_path[0] != '\0');

    {
        size_t i;
        int saw_mounted = 0;
        int saw_ps3_iso_id = 0;
        int saw_ps2_cover = 0;
        for (i = 0; i < list.count; i++) {
            if (strcmp(list.items[i].title_id, "BLES12345") == 0)
                saw_mounted = 1;
            if (strcmp(list.items[i].title_id, "BCES01893") == 0)
                saw_ps3_iso_id = 1;
            if (strcmp(list.items[i].title, "Demo PS2 Classic") == 0 &&
                list.items[i].cover_path[0] != '\0')
                saw_ps2_cover = 1;
        }
        assert(saw_mounted);
        assert(saw_ps3_iso_id);
        assert(saw_ps2_cover);
    }

    puts("PS3 SCALE scanner tests: PASS");
    return 0;
}
