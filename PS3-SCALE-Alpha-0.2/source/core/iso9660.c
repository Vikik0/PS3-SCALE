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
#define ISO_MAX_FILE_EXTENTS 32u

typedef struct {
    uint32_t lba;
    uint32_t size;
} iso_file_extent_t;

typedef struct {
    iso_file_extent_t extents[ISO_MAX_FILE_EXTENTS];
    size_t extent_count;
    size_t total_size;
    unsigned char flags;
} iso_file_t;

static int find_root_file(const unsigned char *root, size_t root_size,
                          const char *wanted, size_t max_size,
                          iso_file_t *file, unsigned char *flags);
static int read_iso_file(FILE *fp, const iso_file_t *file,
                         unsigned char *out, size_t capacity);

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
    iso_file_t cnf_file;
    unsigned char cnf_flags = 0;
    uint32_t root_lba;
    uint32_t root_size;
    int result = -1;

    if (!iso_path || !out || out_size < 12u)
        return -1;
    out[0] = '\0';

    fp = fopen(iso_path, "rb");
    if (!fp)
        return -2;

    if (read_at(fp, (uint64_t)ISO_PVD_LBA * ISO_SECTOR_SIZE,
                pvd, sizeof(pvd)) != 0)
        goto cleanup;
    if (pvd[0] != 1u || memcmp(pvd + 1u, "CD001", 5u) != 0 ||
        pvd[6] != 1u || pvd[156] < 34u)
        goto cleanup;

    root_lba = read_le32(pvd + 158u);
    root_size = read_le32(pvd + 166u);
    if (!root_lba || root_size == 0u || root_size > ISO_MAX_ROOT_BYTES)
        goto cleanup;

    root = (unsigned char *)malloc(root_size);
    if (!root)
        goto cleanup;
    if (read_at(fp, (uint64_t)root_lba * ISO_SECTOR_SIZE,
                root, root_size) != 0)
        goto cleanup;

    if (find_root_file(root, root_size, "SYSTEM.CNF;1",
                       ISO_MAX_SYSTEM_CNF, &cnf_file, &cnf_flags) != 0 ||
        (cnf_flags & 0x02u) != 0u || cnf_file.total_size == 0u)
        goto cleanup;

    system_cnf = (unsigned char *)malloc(cnf_file.total_size);
    if (!system_cnf)
        goto cleanup;
    if (read_iso_file(fp, &cnf_file, system_cnf, cnf_file.total_size) != 0)
        goto cleanup;

    result = serial_at(system_cnf, cnf_file.total_size, out, out_size);

cleanup:
    free(system_cnf);
    free(root);
    if (fp)
        fclose(fp);
    if (result != 0)
        out[0] = '\0';
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

static int iso_component_equal_ci(const unsigned char *name, size_t name_len,
                                  const char *wanted)
{
    size_t wanted_len;

    if (!name || !wanted)
        return 0;

    wanted_len = strlen(wanted);
    if (name_len == wanted_len)
        return equal_ci_n((const char *)name, wanted, wanted_len);

    /* ISO9660 ;1 version identifiers may be present or omitted in either source. */
    if (name_len == wanted_len + 2u &&
        name[name_len - 2u] == ';' && name[name_len - 1u] == '1')
        return equal_ci_n((const char *)name, wanted, wanted_len);

    if (wanted_len == name_len + 2u &&
        wanted[wanted_len - 2u] == ';' && wanted[wanted_len - 1u] == '1')
        return equal_ci_n((const char *)name, wanted, name_len);

    return 0;
}

static int next_directory_record(const unsigned char *directory,
                                 size_t directory_size, size_t *position,
                                 const unsigned char **record,
                                 size_t *record_length)
{
    if (!directory || !position || !record || !record_length)
        return -1;

    while (*position < directory_size) {
        size_t pos = *position;
        unsigned int length;

        if (directory[pos] == 0u) {
            size_t next_sector =
                ((pos / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            if (next_sector <= pos)
                return -1;
            *position = next_sector;
            continue;
        }

        length = directory[pos];
        if (length < 34u || length > directory_size - pos ||
            length > ISO_SECTOR_SIZE - (pos % ISO_SECTOR_SIZE))
            return -1;

        *record = directory + pos;
        *record_length = length;
        *position = pos + length;
        return 0;
    }

    return 1;
}

static int collect_extent_chain(const unsigned char *directory,
                                size_t directory_size,
                                const unsigned char *first_record,
                                size_t next_position,
                                const char *component,
                                size_t max_size,
                                iso_file_t *file,
                                unsigned char *entry_flags)
{
    const unsigned char *record = first_record;
    const unsigned char *first_name;
    size_t first_name_len;
    size_t position = next_position;
    size_t total_size = 0;
    unsigned char base_flags;
    size_t count = 0;

    if (!directory || !record || !component || !file || !entry_flags ||
        !component[0] || max_size == 0u)
        return -1;

    first_name_len = record[32];
    if (first_name_len == 0u || 33u + first_name_len > record[0] ||
        !iso_component_equal_ci(record + 33u, first_name_len, component))
        return -1;
    first_name = record + 33u;
    base_flags = (unsigned char)(record[25] & 0x7Fu);

    /* Only hidden and directory flags are supported for boot/config files. */
    if ((base_flags & 0x7Cu) != 0u)
        return -1;

    memset(file, 0, sizeof(*file));

    for (;;) {
        unsigned int record_length;
        unsigned int name_length;
        uint32_t extent_lba;
        uint32_t extent_size;
        unsigned char flags;

        record_length = record[0];
        name_length = record[32];
        flags = record[25];

        if (record_length < 34u ||
            33u + name_length > record_length ||
            name_length != first_name_len ||
            memcmp(record + 33u, first_name, first_name_len) != 0 ||
            (flags & 0x7Fu) != base_flags ||
            record[1] != 0u || record[26] != 0u || record[27] != 0u)
            return -1;

        /* Directories and interleaved files cannot be read as plain extents. */
        if ((base_flags & 0x02u) != 0u && (flags & 0x80u) != 0u)
            return -1;

        extent_lba = read_le32(record + 2u);
        extent_size = read_le32(record + 10u);
        if (extent_lba == 0u || extent_size == 0u ||
            (size_t)extent_size > max_size ||
            total_size > max_size - (size_t)extent_size ||
            count >= ISO_MAX_FILE_EXTENTS)
            return -1;

        /* Every non-final extent must end on a logical-sector boundary. */
        if ((flags & 0x80u) != 0u &&
            (extent_size % ISO_SECTOR_SIZE) != 0u)
            return -1;

        file->extents[count].lba = extent_lba;
        file->extents[count].size = extent_size;
        total_size += extent_size;
        ++count;

        if ((flags & 0x80u) == 0u)
            break;

        {
            size_t next_length = 0;
            int next_result = next_directory_record(
                directory, directory_size, &position,
                &record, &next_length);
            if (next_result != 0 || next_length < 34u)
                return -1;
        }
    }

    file->extent_count = count;
    file->total_size = total_size;
    file->flags = base_flags;
    *entry_flags = base_flags;
    return 0;
}

static int find_directory_entry(const unsigned char *directory,
                                size_t directory_size,
                                const char *component,
                                size_t max_file_size,
                                iso_file_t *file,
                                unsigned char *entry_flags)
{
    size_t position = 0;

    if (!directory || !component || !component[0] ||
        !file || !entry_flags || max_file_size == 0u)
        return -1;

    while (position < directory_size) {
        const unsigned char *record = NULL;
        size_t record_length = 0;
        size_t next_position = position;
        unsigned int name_length;
        int next_result = next_directory_record(
            directory, directory_size, &next_position,
            &record, &record_length);

        if (next_result != 0)
            return -1;

        name_length = record[32];
        if (name_length > 0u && 33u + name_length <= record_length &&
            !(name_length == 1u &&
              (record[33] == 0u || record[33] == 1u)) &&
            iso_component_equal_ci(record + 33u, name_length, component)) {
            return collect_extent_chain(
                directory, directory_size, record, next_position,
                component, max_file_size, file, entry_flags);
        }

        position = next_position;
    }

    return -1;
}

static int find_root_file(const unsigned char *root, size_t root_size,
                          const char *wanted, size_t max_size,
                          iso_file_t *file, unsigned char *flags)
{
    if (find_directory_entry(root, root_size, wanted, max_size,
                             file, flags) != 0)
        return -1;
    return (*flags & 0x02u) == 0u ? 0 : -1;
}

static int read_iso_file(FILE *fp, const iso_file_t *file,
                         unsigned char *out, size_t capacity)
{
    size_t total = 0;
    size_t i;

    if (!fp || !file || !out || file->extent_count == 0u ||
        file->extent_count > ISO_MAX_FILE_EXTENTS ||
        file->total_size == 0u || file->total_size > capacity)
        return -1;

    for (i = 0; i < file->extent_count; ++i) {
        size_t extent_size = file->extents[i].size;
        uint64_t offset = (uint64_t)file->extents[i].lba * ISO_SECTOR_SIZE;

        if (file->extents[i].lba == 0u || extent_size == 0u ||
            extent_size > capacity - total ||
            read_at(fp, offset, out + total, extent_size) != 0)
            return -1;
        total += extent_size;
    }

    return total == file->total_size ? 0 : -1;
}

static int find_iso_path(FILE *fp, uint32_t root_lba, uint32_t root_size,
                         const char *path, iso_file_t *file_out)
{
    char path_copy[128];
    char *component;
    uint32_t directory_lba = root_lba;
    uint32_t directory_size = root_size;
    unsigned int depth = 0;
    size_t path_len;

    if (!fp || !path || !file_out ||
        !root_lba || !root_size || root_size > ISO_MAX_ROOT_BYTES)
        return -1;

    path_len = strlen(path);
    if (path_len == 0u || path_len >= sizeof(path_copy))
        return -1;
    memcpy(path_copy, path, path_len + 1u);

    /* SYSTEM.CNF paths are relative to the disc root after cdrom0:. */
    if (path_copy[0] == '/' || path_copy[0] == '\\')
        return -1;

    component = path_copy;
    while (component && component[0]) {
        char *separator = strchr(component, '/');
        int is_last = separator == NULL;
        unsigned char *directory;
        unsigned char flags = 0;
        iso_file_t entry;
        int lookup_result;

        if (separator)
            *separator = '\0';
        if (!component[0] || strcmp(component, ".") == 0 ||
            strcmp(component, "..") == 0 || strchr(component, ':') ||
            strchr(component, '\\') || ++depth > 16u)
            return -1;

        if (directory_size == 0u ||
            directory_size > ISO_MAX_ROOT_BYTES)
            return -1;

        directory = (unsigned char *)malloc(directory_size);
        if (!directory)
            return -1;
        if (read_at(fp, (uint64_t)directory_lba * ISO_SECTOR_SIZE,
                    directory, directory_size) != 0) {
            free(directory);
            return -1;
        }

        lookup_result = find_directory_entry(
            directory, directory_size, component,
            ISO_MAX_BOOT_ELF_BYTES, &entry, &flags);
        free(directory);
        if (lookup_result != 0)
            return -1;

        if (is_last) {
            if (flags & 0x02u)
                return -1;
            *file_out = entry;
            return 0;
        }

        if (!(flags & 0x02u) || entry.extent_count != 1u ||
            entry.total_size > ISO_MAX_ROOT_BYTES)
            return -1;

        directory_lba = entry.extents[0].lba;
        directory_size = (uint32_t)entry.total_size;
        component = separator + 1u;
        if (!component[0])
            return -1;
    }

    return -1;
}

static int boot_filename_from_cnf(const unsigned char *data, size_t size,
                                  char *out, size_t out_size)
{
    size_t pos = 0;

    if (!data || !out || out_size < 8u)
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
        size_t output_len = 0;
        size_t component_start = 0;
        unsigned int depth = 0;
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

        /*
         * Keep the full relative path. Separators are normalized to '/',
         * while absolute paths, empty components and traversal are rejected.
         */
        for (i = value_start; i < value_end; ++i) {
            unsigned char ch = data[i];
            if (ch == '\\' || ch == '/') {
                if (output_len == 0u || out[output_len - 1u] == '/')
                    return -1;
                if (output_len + 1u >= out_size)
                    return -1;
                out[output_len++] = '/';
            } else {
                if (ch == ':' || ch < 0x20u || ch == 0x7Fu ||
                    output_len + 1u >= out_size)
                    return -1;
                out[output_len++] = (char)toupper(ch);
            }
        }

        if (output_len == 0u || out[output_len - 1u] == '/')
            return -1;
        out[output_len] = '\0';

        for (i = 0; i <= output_len; ++i) {
            if (i == output_len || out[i] == '/') {
                size_t component_len = i - component_start;
                if (component_len == 0u ||
                    (component_len == 1u && out[component_start] == '.') ||
                    (component_len == 2u && out[component_start] == '.' &&
                     out[component_start + 1u] == '.'))
                    return -1;
                if (++depth > 16u)
                    return -1;
                component_start = i + 1u;
            }
        }

        /* Add ISO9660's common ;1 file version only to the final component. */
        {
            const char *last_separator = strrchr(out, '/');
            size_t final_start = last_separator
                ? (size_t)(last_separator - out) + 1u : 0u;
            if (memchr(out + final_start, ';', output_len - final_start) == NULL) {
                if (output_len + 3u > out_size)
                    return -1;
                out[output_len++] = ';';
                out[output_len++] = '1';
                out[output_len] = '\0';
            }
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
    uint32_t root_lba, root_size;
    iso_file_t cnf_file, boot_file;
    unsigned char cnf_flags = 0;
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
                       ISO_MAX_SYSTEM_CNF, &cnf_file, &cnf_flags) != 0 ||
        (cnf_flags & 0x02u) != 0u || cnf_file.total_size == 0u)
        goto cleanup;

    system_cnf = (unsigned char *)malloc(cnf_file.total_size);
    if (!system_cnf)
        goto cleanup;
    if (read_iso_file(fp, &cnf_file, system_cnf, cnf_file.total_size) != 0)
        goto cleanup;

    if (serial_at(system_cnf, cnf_file.total_size, serial, serial_size) != 0 ||
        boot_filename_from_cnf(system_cnf, cnf_file.total_size,
                               boot_name, sizeof(boot_name)) != 0)
        goto cleanup;

    if (find_iso_path(fp, root_lba, root_size, boot_name,
                      &boot_file) != 0 ||
        boot_file.total_size < 52u ||
        boot_file.total_size > ISO_MAX_BOOT_ELF_BYTES)
        goto cleanup;

    elf = (unsigned char *)malloc(boot_file.total_size);
    if (!elf)
        goto cleanup;
    if (read_iso_file(fp, &boot_file, elf, boot_file.total_size) != 0 ||
        validate_ps2_elf(elf, boot_file.total_size) != 0)
        goto cleanup;

    for (pos = 0; pos + 4u <= boot_file.total_size; pos += 4u)
        hash ^= read_le32(elf + pos);

    *elf_data = elf;
    *elf_size = boot_file.total_size;
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
