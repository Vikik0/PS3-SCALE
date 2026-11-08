#include "ps3scale/sfo.h"
#include "ps3scale/game.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static size_t add_entry(unsigned char *buf, size_t entry_index,
                        unsigned short key_offset, const char *value,
                        unsigned int value_offset)
{
    unsigned char *e = buf + 20 + entry_index * 16;
    size_t len = strlen(value) + 1;
    put16(e, key_offset);
    put16(e + 2, 0x0204);
    put32(e + 4, (unsigned int)len);
    put32(e + 8, (unsigned int)len);
    put32(e + 12, value_offset);
    return len;
}

static size_t make_sfo(unsigned char *buf, size_t cap)
{
    const size_t entry_bytes = 4 * 16;
    const size_t key_off = 20 + entry_bytes;
    const char keys[] = "TITLE\0TITLE_ID\0CATEGORY\0APP_VER\0";
    const size_t data_off = key_off + sizeof(keys);
    size_t p = data_off;
    size_t l0, l1, l2, l3;

    assert(cap >= 256);
    memset(buf, 0, cap);
    buf[0] = 0; buf[1] = 'P'; buf[2] = 'S'; buf[3] = 'F';
    put32(buf + 4, 0x00000101);
    put32(buf + 8, (unsigned int)key_off);
    put32(buf + 12, (unsigned int)data_off);
    put32(buf + 16, 4);
    memcpy(buf + key_off, keys, sizeof(keys));

    l0 = add_entry(buf, 0, 0, "Sample PS3 Game", (unsigned int)(p - data_off));
    memcpy(buf + p, "Sample PS3 Game", l0); p += l0;
    l1 = add_entry(buf, 1, 6, "BLUS12345", (unsigned int)(p - data_off));
    memcpy(buf + p, "BLUS12345", l1); p += l1;
    l2 = add_entry(buf, 2, 15, "DG", (unsigned int)(p - data_off));
    memcpy(buf + p, "DG", l2); p += l2;
    l3 = add_entry(buf, 3, 24, "01.00", (unsigned int)(p - data_off));
    memcpy(buf + p, "01.00", l3); p += l3;
    return p;
}

int main(void)
{
    unsigned char buffer[512];
    ps3scale_sfo_info_t info;
    size_t size = make_sfo(buffer, sizeof(buffer));

    assert(ps3scale_sfo_parse(buffer, size, &info) == 0);
    assert(strcmp(info.title, "Sample PS3 Game") == 0);
    assert(strcmp(info.title_id, "BLUS12345") == 0);
    assert(strcmp(info.category, "DG") == 0);
    assert(strcmp(info.app_ver, "01.00") == 0);

    buffer[0] = 'X';
    assert(ps3scale_sfo_parse(buffer, size, &info) < 0);
    assert(ps3scale_title_id_valid("BLUS12345"));
    assert(!ps3scale_title_id_valid("BLUS1234"));
    assert(!ps3scale_title_id_valid("../etc/pass"));

    puts("PS3 SCALE host tests: PASS");
    return 0;
}
