#include "ps3scale/sfo.h"

#include <string.h>

#define SFO_HEADER_SIZE 20u
#define SFO_ENTRY_SIZE  16u
#define SFO_MAX_ENTRIES 1024u

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int range_ok(size_t total, size_t offset, size_t amount)
{
    return offset <= total && amount <= (total - offset);
}

static void copy_sfo_string(char *dst, size_t dst_size,
                            const uint8_t *src, size_t src_size)
{
    size_t n = 0;
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    while (n < src_size && src[n] != 0) n++;
    if (n >= dst_size) n = dst_size - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';
}

int ps3scale_title_id_valid(const char *id)
{
    size_t i;
    if (!id || strlen(id) != 9) return 0;
    for (i = 0; i < 9; i++) {
        const char c = id[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

int ps3scale_sfo_parse(const uint8_t *data, size_t size,
                       ps3scale_sfo_info_t *out)
{
    uint32_t key_table, data_table, entry_count;
    size_t entries_bytes, i;
    int found_title_id = 0;

    if (!data || !out || size < SFO_HEADER_SIZE) return -1;
    memset(out, 0, sizeof(*out));

    /* PARAM.SFO magic is the four bytes: 00 50 53 46 ("\\0PSF"). */
    if (data[0] != 0x00 || data[1] != 'P' ||
        data[2] != 'S' || data[3] != 'F') return -2;

    key_table  = read_le32(data + 8);
    data_table = read_le32(data + 12);
    entry_count = read_le32(data + 16);

    if (entry_count == 0 || entry_count > SFO_MAX_ENTRIES) return -3;
    entries_bytes = (size_t)entry_count * SFO_ENTRY_SIZE;
    if (!range_ok(size, SFO_HEADER_SIZE, entries_bytes)) return -5;
    if (key_table < SFO_HEADER_SIZE + entries_bytes ||
        data_table < key_table || data_table > size) return -4;

    for (i = 0; i < entry_count; i++) {
        const uint8_t *e = data + SFO_HEADER_SIZE + i * SFO_ENTRY_SIZE;
        uint16_t key_off = read_le16(e);
        uint16_t format = read_le16(e + 2);
        uint32_t data_len = read_le32(e + 4);
        uint32_t data_max = read_le32(e + 8);
        uint32_t value_off = read_le32(e + 12);
        size_t key_pos = (size_t)key_table + key_off;
        size_t value_pos = (size_t)data_table + value_off;
        const uint8_t *key_end;
        const uint8_t *value;
        const char *key;

        if (key_pos >= data_table || key_pos >= size) continue;
        key_end = (const uint8_t *)memchr(data + key_pos, 0,
                                          (size_t)data_table - key_pos);
        if (!key_end) continue;
        key = (const char *)(data + key_pos);

        if (data_len > data_max ||
            !range_ok(size, value_pos, data_len)) continue;

        /* String parameters use format 0x0204. Ignore non-string entries. */
        if (format != 0x0204u) continue;
        value = data + value_pos;

        if (strcmp(key, "TITLE") == 0) {
            copy_sfo_string(out->title, sizeof(out->title), value, data_len);
        } else if (strcmp(key, "TITLE_ID") == 0) {
            copy_sfo_string(out->title_id, sizeof(out->title_id),
                            value, data_len);
            found_title_id = ps3scale_title_id_valid(out->title_id);
        } else if (strcmp(key, "CATEGORY") == 0) {
            copy_sfo_string(out->category, sizeof(out->category),
                            value, data_len);
        } else if (strcmp(key, "APP_VER") == 0) {
            copy_sfo_string(out->app_ver, sizeof(out->app_ver),
                            value, data_len);
        }
    }

    if (!found_title_id) return -6;
    return 0;
}
