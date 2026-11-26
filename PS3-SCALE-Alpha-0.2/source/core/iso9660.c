#include "ps3scale/iso9660.h"

#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ISO_SECTOR_SIZE 2048u
#define ISO_PVD_LBA    16u
#define ISO_MAX_ROOT_BYTES (1024u * 1024u)
#define ISO_MAX_SYSTEM_CNF  (16u * 1024u)

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int read_at(FILE *fp, uint64_t offset, void *dst, size_t bytes)
{
    if (!fp || !dst || offset > (uint64_t)LONG_MAX) return -1;
    if (fseek(fp, (long)offset, SEEK_SET) != 0) return -1;
    return fread(dst, 1, bytes, fp) == bytes ? 0 : -1;
}

static int equal_ci_n(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) !=
            tolower((unsigned char)b[i])) return 0;
    }
    return 1;
}

static int serial_at(const unsigned char *p, size_t left,
                     char *out, size_t out_size)
{
    size_t i;
    if (!p || !out || out_size < 12) return -1;

    for (i = 0; i + 11 <= left; i++) {
        size_t j;
        int valid = 1;

        for (j = 0; j < 4; j++) {
            if (!isalpha((unsigned char)p[i + j])) {
                valid = 0;
                break;
            }
        }
        if (!valid || p[i + 4] != '_') continue;
        for (j = 5; j < 8; j++) {
            if (!isdigit((unsigned char)p[i + j])) {
                valid = 0;
                break;
            }
        }
        if (!valid || p[i + 8] != '.') continue;
        if (!isdigit((unsigned char)p[i + 9]) ||
            !isdigit((unsigned char)p[i + 10])) continue;

        for (j = 0; j < 11; j++)
            out[j] = (char)toupper((unsigned char)p[i + j]);
        out[11] = '\0';
        return 0;
    }
    return -1;
}

int ps3scale_ps2_iso_read_serial(const char *iso_path,
                                 char *out, size_t out_size)
{
    FILE *fp = NULL;
    unsigned char pvd[ISO_SECTOR_SIZE];
    unsigned char *root = NULL;
    unsigned char *system_cnf = NULL;
    uint32_t root_lba, root_size, cnf_lba, cnf_size;
    size_t pos;
    int result = -1;

    if (!iso_path || !out || out_size < 12) return -1;
    out[0] = '\0';

    fp = fopen(iso_path, "rb");
    if (!fp) return -2;

    if (read_at(fp, (uint64_t)ISO_PVD_LBA * ISO_SECTOR_SIZE,
                pvd, sizeof(pvd)) != 0)
        goto cleanup;

    /* ISO9660 Primary Volume Descriptor: type 1, identifier CD001, version 1. */
    if (pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0 || pvd[6] != 1)
        goto cleanup;

    /* Root directory record begins at byte 156 in the PVD. */
    if (pvd[156] < 34) goto cleanup;
    root_lba = read_le32(pvd + 158);
    root_size = read_le32(pvd + 166);
    if (root_size == 0 || root_size > ISO_MAX_ROOT_BYTES)
        goto cleanup;

    root = (unsigned char *)malloc(root_size);
    if (!root) goto cleanup;
    if (read_at(fp, (uint64_t)root_lba * ISO_SECTOR_SIZE,
                root, root_size) != 0)
        goto cleanup;

    cnf_lba = 0;
    cnf_size = 0;
    pos = 0;

    while (pos < root_size) {
        unsigned int rec_len;
        unsigned int name_len;
        const unsigned char *rec;
        const unsigned char *name;

        if (root[pos] == 0) {
            pos = ((pos / ISO_SECTOR_SIZE) + 1) * ISO_SECTOR_SIZE;
            continue;
        }

        rec_len = root[pos];
        if (rec_len < 34 || pos + rec_len > root_size)
            break;

        rec = root + pos;
        name_len = rec[32];
        if (name_len > 0 && 33u + name_len <= rec_len) {
            name = rec + 33;
            if (!(name_len == 1 && (name[0] == 0 || name[0] == 1)) &&
                name_len >= 11 &&
                equal_ci_n((const char *)name, "SYSTEM.CNF", 10) &&
                (name_len == 10 || name[10] == ';')) {
                /* Ignore directory records; SYSTEM.CNF must be a file. */
                if ((rec[25] & 0x02u) == 0) {
                    cnf_lba = read_le32(rec + 2);
                    cnf_size = read_le32(rec + 10);
                    break;
                }
            }
        }
        pos += rec_len;
    }

    if (!cnf_lba || !cnf_size || cnf_size > ISO_MAX_SYSTEM_CNF)
        goto cleanup;

    system_cnf = (unsigned char *)malloc(cnf_size);
    if (!system_cnf) goto cleanup;
    if (read_at(fp, (uint64_t)cnf_lba * ISO_SECTOR_SIZE,
                system_cnf, cnf_size) != 0)
        goto cleanup;

    result = serial_at(system_cnf, cnf_size, out, out_size);

cleanup:
    free(system_cnf);
    free(root);
    if (fp) fclose(fp);
    return result;
}

int ps3scale_title_id_from_filename(const char *filename,
                                    char *out, size_t out_size)
{
    const char *p;
    size_t i;

    if (!filename || !out || out_size < 10) return -1;
    out[0] = '\0';

    for (p = filename; *p; p++) {
        int valid = 1;
        if (strlen(p) < 9) break;
        for (i = 0; i < 9; i++) {
            unsigned char c = (unsigned char)p[i];
            if (!c) { valid = 0; break; }
            if (i < 4) {
                if (!isalpha(c)) { valid = 0; break; }
            } else if (!isdigit(c)) {
                valid = 0;
                break;
            }
        }
        if (valid) {
            for (i = 0; i < 9; i++)
                out[i] = (char)toupper((unsigned char)p[i]);
            out[9] = '\0';
            return 0;
        }
    }
    return -1;
}

int ps3scale_ps2_serial_from_filename(const char *filename,
                                      char *out, size_t out_size)
{
    const char *p;
    size_t i;

    if (!filename || !out || out_size < 12) return -1;
    out[0] = '\0';

    for (p = filename; *p; p++) {
        int valid = 1;
        if (strlen(p) < 11) break;
        for (i = 0; i < 11; i++) {
            unsigned char c = (unsigned char)p[i];
            if (!c) { valid = 0; break; }
            if (i < 4) {
                if (!isalpha(c)) { valid = 0; break; }
            } else if (i == 4) {
                if (c != '_') { valid = 0; break; }
            } else if (i < 8 || i > 8) {
                if (!isdigit(c)) { valid = 0; break; }
            } else if (c != '.') {
                valid = 0;
                break;
            }
        }
        if (valid) {
            for (i = 0; i < 11; i++)
                out[i] = (char)toupper((unsigned char)p[i]);
            out[11] = '\0';
            return 0;
        }
    }
    return -1;
}

#define ISO_MAX_BOOT_ELF_BYTES (32u * 1024u * 1024u)

static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int iso_name_equal_ci(const unsigned char *name, size_t name_len,
                             const char *wanted)
{
    size_t wanted_len;

    if (!name || !wanted)
        return 0;
    wanted_len = strlen(wanted);
    if (name_len != wanted_len)
        return 0;
    return equal_ci_n((const char *)name, wanted, wanted_len);
}

static int find_root_file(const unsigned char *root, size_t root_size,
                          const char *wanted, uint32_t *lba,
                          uint32_t *file_size)
{
    size_t pos = 0;

    if (!root || !wanted || !lba || !file_size)
        return -1;

    while (pos < root_size) {
        unsigned int rec_len;
        unsigned int name_len;
        const unsigned char *rec;

        if (root[pos] == 0) {
            pos = ((pos / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            continue;
        }

        rec_len = root[pos];
        if (rec_len < 34u || pos + rec_len > root_size)
            return -1;

        rec = root + pos;
        name_len = rec[32];
        if (name_len > 0 && 33u + name_len <= rec_len &&
            !(name_len == 1u && (rec[33] == 0 || rec[33] == 1)) &&
            iso_name_equal_ci(rec + 33, name_len, wanted) &&
            (rec[25] & 0x02u) == 0) {
            *lba = read_le32(rec + 2u);
            *file_size = read_le32(rec + 10u);
            return (*lba && *file_size) ? 0 : -1;
        }

        pos += rec_len;
    }

    return -1;
}

static int boot_filename_from_cnf(const unsigned char *data, size_t size,
                                  char *out, size_t out_size)
{
    size_t pos = 0;

    if (!data || !out || out_size < 4u)
        return -1;
    out[0] = '\0';

    while (pos < size) {
        size_t line_start = pos;
        size_t line_end;
        size_t key_start;
        size_t key_len;
        size_t value_start;
        size_t value_end;
        size_t colon;
        size_t i;
        size_t name_len;
        int is_boot_key;

        while (pos < size && data[pos] != '\n' && data[pos] != '\r')
            ++pos;
        line_end = pos;
        while (pos < size && (data[pos] == '\n' || data[pos] == '\r'))
            ++pos;

        key_start = line_start;
        while (key_start < line_end &&
               (data[key_start] == ' ' || data[key_start] == '\t'))
            ++key_start;
        key_len = line_end - key_start;
        is_boot_key =
            (key_len >= 5u &&
             equal_ci_n((const char *)data + key_start, "BOOT2", 5u) &&
             (key_len == 5u || data[key_start + 5u] == ' ' ||
              data[key_start + 5u] == '\t' || data[key_start + 5u] == '=')) ||
            (key_len >= 4u &&
             equal_ci_n((const char *)data + key_start, "BOOT", 4u) &&
             (key_len == 4u || data[key_start + 4u] == ' ' ||
              data[key_start + 4u] == '\t' || data[key_start + 4u] == '='));
        if (!is_boot_key)
            continue;

        while (key_start < line_end && data[key_start] != '=')
            ++key_start;
        if (key_start == line_end)
            continue;

        value_start = key_start + 1u;
        while (value_start < line_end &&
               (data[value_start] == ' ' || data[value_start] == '\t'))
            ++value_start;

        colon = value_start;
        while (colon < line_end && data[colon] != ':')
            ++colon;
        if (colon == line_end)
            return -1;

        value_start = colon + 1u;
        while (value_start < line_end &&
               (data[value_start] == '\\' || data[value_start] == '/'))
            ++value_start;

        value_end = line_end;
        while (value_end > value_start &&
               (data[value_end - 1u] == ' ' ||
                data[value_end - 1u] == '\t'))
            --value_end;
        if (value_start >= value_end)
            return -1;

        /* This first implementation only supports root-level boot ELFs. */
        for (i = value_start; i < value_end; ++i) {
            if (data[i] == '\\' || data[i] == '/')
                return -2;
        }

        name_len = value_end - value_start;
        if (name_len + 3u >= out_size)
            return -1;
        for (i = 0; i < name_len; ++i)
            out[i] = (char)toupper((unsigned char)data[value_start + i]);
        out[name_len] = '\0';

        if (strchr(out, ';') == NULL) {
            out[name_len++] = ';';
            out[name_len++] = '1';
            out[name_len] = '\0';
        }
        return 0;
    }

    return -1;
}

static int validate_ps2_elf(const unsigned char *elf, size_t size)
{
    if (!elf || size < 52u ||
        memcmp(elf, "\x7F" "ELF", 4u) != 0 ||
        elf[4] != 1u || elf[5] != 1u || elf[6] != 1u ||
        read_le16(elf + 18u) != 8u)
        return -1;
    return 0;
}

int ps3scale_ps2_iso_load_boot_elf(const char *iso_path,
                                  unsigned char **elf_data,
                                  size_t *elf_size,
                                  char *serial, size_t serial_size,
                                  uint32_t *pcsx2_elf_hash)
{
    FILE *fp = NULL;
    unsigned char pvd[ISO_SECTOR_SIZE];
    unsigned char *root = NULL;
    unsigned char *system_cnf = NULL;
    unsigned char *elf = NULL;
    uint32_t root_lba, root_size, cnf_lba, cnf_size, elf_lba, boot_size;
    char boot_name[128];
    size_t pos;
    uint32_t hash = 0;
    int result = -1;

    if (elf_data)
        *elf_data = NULL;
    if (elf_size)
        *elf_size = 0;
    if (pcsx2_elf_hash)
        *pcsx2_elf_hash = 0;
    if (!iso_path || !elf_data || !elf_size || !serial ||
        serial_size < 12u || !pcsx2_elf_hash)
        return -1;
    serial[0] = '\0';

    fp = fopen(iso_path, "rb");
    if (!fp)
        return -2;

    if (read_at(fp, (uint64_t)ISO_PVD_LBA * ISO_SECTOR_SIZE,
                pvd, sizeof(pvd)) != 0)
        goto cleanup;
    if (pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0 || pvd[6] != 1)
        goto cleanup;
    if (pvd[156] < 34u)
        goto cleanup;

    root_lba = read_le32(pvd + 158u);
    root_size = read_le32(pvd + 166u);
    if (!root_lba || root_size == 0 || root_size > ISO_MAX_ROOT_BYTES)
        goto cleanup;

    root = (unsigned char *)malloc(root_size);
    if (!root)
        goto cleanup;
    if (read_at(fp, (uint64_t)root_lba * ISO_SECTOR_SIZE,
                root, root_size) != 0)
        goto cleanup;

    if (find_root_file(root, root_size, "SYSTEM.CNF;1",
                       &cnf_lba, &cnf_size) != 0 ||
        cnf_size == 0 || cnf_size > ISO_MAX_SYSTEM_CNF)
        goto cleanup;

    system_cnf = (unsigned char *)malloc(cnf_size);
    if (!system_cnf)
        goto cleanup;
    if (read_at(fp, (uint64_t)cnf_lba * ISO_SECTOR_SIZE,
                system_cnf, cnf_size) != 0)
        goto cleanup;

    if (serial_at(system_cnf, cnf_size, serial, serial_size) != 0 ||
        boot_filename_from_cnf(system_cnf, cnf_size,
                               boot_name, sizeof(boot_name)) != 0)
        goto cleanup;

    if (find_root_file(root, root_size, boot_name,
                       &elf_lba, &boot_size) != 0 ||
        boot_size < 52u || boot_size > ISO_MAX_BOOT_ELF_BYTES)
        goto cleanup;

    elf = (unsigned char *)malloc(boot_size);
    if (!elf)
        goto cleanup;
    if (read_at(fp, (uint64_t)elf_lba * ISO_SECTOR_SIZE,
                elf, boot_size) != 0 ||
        validate_ps2_elf(elf, boot_size) != 0)
        goto cleanup;

    for (pos = 0; pos + 4u <= boot_size; pos += 4u)
        hash ^= read_le32(elf + pos);

    *elf_data = elf;
    *elf_size = boot_size;
    *pcsx2_elf_hash = hash;
    elf = NULL;
    result = 0;

cleanup:
    free(elf);
    free(system_cnf);
    free(root);
    if (fp)
        fclose(fp);
    if (result != 0)
        serial[0] = '\0';
    return result;
}

int ps3scale_ps2_elf_read_exec_word(const unsigned char *elf_data,
                                   size_t elf_size,
                                   uint32_t ee_address,
                                   uint32_t *word_out)
{
    uint32_t phoff;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t i;

    if (!word_out || validate_ps2_elf(elf_data, elf_size) != 0 ||
        (ee_address & 3u) != 0 || ee_address >= 0x10000000u)
        return -1;

    phoff = read_le32(elf_data + 28u);
    phentsize = read_le16(elf_data + 42u);
    phnum = read_le16(elf_data + 44u);
    if (phentsize < 32u || phnum == 0 || phnum > 64u ||
        (uint64_t)phoff + (uint64_t)phentsize * phnum > elf_size)
        return -2;

    for (i = 0; i < phnum; ++i) {
        const unsigned char *ph = elf_data + phoff + (size_t)i * phentsize;
        uint32_t type = read_le32(ph);
        uint32_t offset = read_le32(ph + 4u);
        uint32_t vaddr = read_le32(ph + 8u);
        uint32_t filesz = read_le32(ph + 16u);
        uint32_t flags = read_le32(ph + 24u);
        uint64_t end_address = (uint64_t)vaddr + filesz;
        uint64_t file_offset;

        if (type != 1u || (flags & 1u) == 0 ||
            ee_address < vaddr ||
            (uint64_t)ee_address + 4u > end_address)
            continue;

        file_offset = (uint64_t)offset + (ee_address - vaddr);
        if (file_offset + 4u > elf_size)
            return -3;

        *word_out = read_le32(elf_data + file_offset);
        return 0;
    }

    return -4;
}
