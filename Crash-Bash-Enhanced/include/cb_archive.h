#ifndef CB_ARCHIVE_H
#define CB_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CB_ARCHIVE_SECTOR_BYTES 2048u

typedef struct CBArchive {
    FILE *file;
    uint64_t size_bytes;
} CBArchive;

typedef struct CBArchiveEntry {
    uint32_t sector;
    uint32_t size_bytes;
} CBArchiveEntry;

/* Opens a user-supplied CRASHBSH.DAT. Returns 0 on success. */
int cb_archive_open(CBArchive *archive, const char *path);
void cb_archive_close(CBArchive *archive);

/* Reads whole 2048-byte sectors. Bytes beyond EOF are zero-filled, matching the PC port's archive reader.
   dst_capacity must cover sector_count * CB_ARCHIVE_SECTOR_BYTES. */
int cb_archive_read_sectors(CBArchive *archive, uint32_t first_sector,
                            uint32_t sector_count, void *dst, size_t dst_capacity);

/* Reads an indexed file and zero-pads its final sector. The caller supplies the original DAT index entry.
   dst_capacity must cover ceil(size_bytes / 2048) * 2048 bytes. */
int cb_archive_read_entry(CBArchive *archive, const CBArchiveEntry *entry,
                          void *dst, size_t dst_capacity);

uint64_t cb_archive_size(const CBArchive *archive);

#endif
