#ifndef GHC_TEST_CELL_FS_FILE_API_H
#define GHC_TEST_CELL_FS_FILE_API_H

#include <stdint.h>

#define CELL_FS_O_RDONLY 0
#define CELL_FS_SUCCEEDED 0

int32_t cellFsOpen(
    const char *path,
    int flags,
    int *fd,
    const void *arg,
    uint64_t size
);
int32_t cellFsRead(int fd, void *buffer, uint64_t bytes_to_read, uint64_t *bytes_read);
int32_t cellFsClose(int fd);

#endif
