#include "cb_archive.h"

#include <limits.h>
#include <string.h>

static int sector_buffer_size(uint32_t sectors, size_t *bytes)
{
    if (bytes == NULL || (uint64_t)sectors * CB_ARCHIVE_SECTOR_BYTES > (uint64_t)SIZE_MAX) {
        return -1;
    }
    *bytes = (size_t)sectors * CB_ARCHIVE_SECTOR_BYTES;
    return 0;
}

int cb_archive_open(CBArchive *archive, const char *path)
{
    long end;

    if (archive == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }
    archive->file = NULL;
    archive->size_bytes = 0;
    archive->file = fopen(path, "rb");
    if (archive->file == NULL) {
        return -1;
    }
    if (fseek(archive->file, 0, SEEK_END) != 0) {
        cb_archive_close(archive);
        return -1;
    }
    end = ftell(archive->file);
    if (end < 0 || fseek(archive->file, 0, SEEK_SET) != 0) {
        cb_archive_close(archive);
        return -1;
    }
    archive->size_bytes = (uint64_t)(unsigned long)end;
    return 0;
}

void cb_archive_close(CBArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    if (archive->file != NULL) {
        fclose(archive->file);
    }
    archive->file = NULL;
    archive->size_bytes = 0;
}

uint64_t cb_archive_size(const CBArchive *archive)
{
    return archive != NULL && archive->file != NULL ? archive->size_bytes : 0;
}

int cb_archive_read_sectors(CBArchive *archive, uint32_t first_sector,
                            uint32_t sector_count, void *dst, size_t dst_capacity)
{
    uint64_t offset;
    size_t wanted;
    size_t got;

    if (archive == NULL || archive->file == NULL || dst == NULL ||
        sector_buffer_size(sector_count, &wanted) != 0 || dst_capacity < wanted) {
        return -1;
    }
    offset = (uint64_t)first_sector * CB_ARCHIVE_SECTOR_BYTES;
    if (offset > (uint64_t)LONG_MAX || fseek(archive->file, (long)offset, SEEK_SET) != 0) {
        return -1;
    }
    got = fread(dst, 1, wanted, archive->file);
    if (got < wanted) {
        if (ferror(archive->file)) {
            clearerr(archive->file);
            return -1;
        }
        memset((unsigned char *)dst + got, 0, wanted - got);
    }
    return 0;
}

int cb_archive_read_entry(CBArchive *archive, const CBArchiveEntry *entry,
                          void *dst, size_t dst_capacity)
{
    uint32_t sectors;
    size_t wanted;

    if (entry == NULL) {
        return -1;
    }
    sectors = entry->size_bytes / CB_ARCHIVE_SECTOR_BYTES;
    if (entry->size_bytes % CB_ARCHIVE_SECTOR_BYTES != 0) {
        if (sectors == UINT32_MAX) {
            return -1;
        }
        sectors++;
    }
    if (sector_buffer_size(sectors, &wanted) != 0 || dst_capacity < wanted) {
        return -1;
    }
    return cb_archive_read_sectors(archive, entry->sector, sectors, dst, dst_capacity);
}
