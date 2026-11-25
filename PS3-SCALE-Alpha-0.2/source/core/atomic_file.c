#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "ps3scale/atomic_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PS3SCALE_ATOMIC_TEMP_PATH_MAX 1024u
#define PS3SCALE_ATOMIC_TEMP_ATTEMPTS 16u
#define PS3SCALE_ATOMIC_WRITE_CHUNK 4096u

static unsigned long g_atomic_file_sequence;

int ps3scale_atomic_file_write(const char *path, const void *data, size_t length)
{
    char temporary_path[PS3SCALE_ATOMIC_TEMP_PATH_MAX];
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset = 0;
    unsigned int attempt;
    int descriptor = -1;
    int written;

    if (!path || !path[0] || (!data && length != 0) ||
        strlen(path) + 80u >= sizeof(temporary_path))
        return -1;

    for (attempt = 0; attempt < PS3SCALE_ATOMIC_TEMP_ATTEMPTS; attempt++) {
        unsigned long sequence = ++g_atomic_file_sequence;
        written = snprintf(temporary_path, sizeof(temporary_path),
                           "%s.ps3scale.%ld.%lu.%lu.tmp",
                           path, (long)getpid(), (unsigned long)time(NULL),
                           sequence);
        if (written < 0 || (size_t)written >= sizeof(temporary_path))
            return -1;

        descriptor = open(temporary_path,
                          O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (descriptor >= 0)
            break;
        if (errno != EEXIST)
            return -1;
    }

    if (descriptor < 0)
        return -1;

    while (offset < length) {
        size_t chunk = length - offset;
        ssize_t count;

        if (chunk > PS3SCALE_ATOMIC_WRITE_CHUNK)
            chunk = PS3SCALE_ATOMIC_WRITE_CHUNK;
        count = write(descriptor, bytes + offset, chunk);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            goto fail;
        offset += (size_t)count;
    }

    if (fsync(descriptor) != 0)
        goto fail;
    if (close(descriptor) != 0) {
        descriptor = -1;
        goto fail_unlink;
    }
    descriptor = -1;

    /* The temp file is beside the destination, so rename is same-volume. */
    if (rename(temporary_path, path) != 0)
        goto fail_unlink;
    return 0;

fail:
    (void)close(descriptor);
    descriptor = -1;
fail_unlink:
    (void)unlink(temporary_path);
    return -1;
}
