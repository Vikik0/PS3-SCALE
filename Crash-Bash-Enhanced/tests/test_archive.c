#include "cb_archive.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE "cb_archive_test.dat"

static void make_test_archive(void)
{
    FILE *f = fopen(TEST_FILE, "wb");
    unsigned char sector[CB_ARCHIVE_SECTOR_BYTES];
    size_t i;

    assert(f != NULL);
    memset(sector, 'A', sizeof(sector));
    assert(fwrite(sector, 1, sizeof(sector), f) == sizeof(sector));
    memset(sector, 'B', sizeof(sector));
    assert(fwrite(sector, 1, sizeof(sector), f) == sizeof(sector));
    for (i = 0; i < 17; i++) sector[i] = (unsigned char)(i + 1);
    assert(fwrite(sector, 1, 17, f) == 17);
    assert(fclose(f) == 0);
}

int main(void)
{
    CBArchive archive = { 0 };
    CBArchiveEntry entry = { 1, CB_ARCHIVE_SECTOR_BYTES + 17 };
    unsigned char *buffer = (unsigned char *)malloc(CB_ARCHIVE_SECTOR_BYTES * 2);
    size_t i;

    assert(buffer != NULL);
    make_test_archive();
    assert(cb_archive_open(&archive, TEST_FILE) == 0);
    assert(cb_archive_size(&archive) == CB_ARCHIVE_SECTOR_BYTES * 2 + 17);

    memset(buffer, 0xCC, CB_ARCHIVE_SECTOR_BYTES * 2);
    assert(cb_archive_read_sectors(&archive, 0, 1, buffer, CB_ARCHIVE_SECTOR_BYTES) == 0);
    for (i = 0; i < CB_ARCHIVE_SECTOR_BYTES; i++) assert(buffer[i] == 'A');

    memset(buffer, 0xCC, CB_ARCHIVE_SECTOR_BYTES * 2);
    assert(cb_archive_read_entry(&archive, &entry, buffer, CB_ARCHIVE_SECTOR_BYTES * 2) == 0);
    for (i = 0; i < CB_ARCHIVE_SECTOR_BYTES; i++) assert(buffer[i] == 'B');
    for (i = 0; i < 17; i++) assert(buffer[CB_ARCHIVE_SECTOR_BYTES + i] == (unsigned char)(i + 1));
    for (i = 17; i < CB_ARCHIVE_SECTOR_BYTES; i++) assert(buffer[CB_ARCHIVE_SECTOR_BYTES + i] == 0);

    assert(cb_archive_read_sectors(&archive, 0, 1, buffer, 1) == -1);
    assert(cb_archive_read_entry(&archive, &entry, buffer, 1) == -1);
    assert(cb_archive_read_sectors(&archive, 3, 1, buffer, CB_ARCHIVE_SECTOR_BYTES) == 0);
    for (i = 0; i < CB_ARCHIVE_SECTOR_BYTES; i++) assert(buffer[i] == 0);

    cb_archive_close(&archive);
    assert(cb_archive_size(&archive) == 0);
    assert(archive.file == NULL);
    free(buffer);
    remove(TEST_FILE);
    puts("Archive reader tests passed.");
    return 0;
}
