#ifndef PS3SCALE_ATOMIC_FILE_H
#define PS3SCALE_ATOMIC_FILE_H

#include <stddef.h>

/*
 * Write a complete file through an exclusive same-directory temporary file,
 * then atomically rename it over the destination. Existing temp paths and
 * symlinks are never followed or removed.
 */
int ps3scale_atomic_file_write(const char *path, const void *data, size_t length);

#endif
