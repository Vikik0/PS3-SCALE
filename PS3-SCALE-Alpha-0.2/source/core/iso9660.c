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
