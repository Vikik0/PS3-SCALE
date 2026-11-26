#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "ps3scale/ps2_config_store.h"
#include "ps3scale/ps2_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COPY_CHUNK 4096u

int ps3scale_ps2_config_store_probe(const char *path, size_t max_bytes,
                                    size_t *file_size)
{
    struct stat st;

    if (file_size)
        *file_size = 0;
    if (!path || !path[0] || max_bytes == 0)
        return -1;
#if defined(PS3SCALE_HOST_TEST)
    if (lstat(path, &st) != 0)
#else
    if (stat(path, &st) != 0)
#endif
        return errno == ENOENT ? 0 : -1;
    if (!S_ISREG(st.st_mode) || st.st_size <= 0 ||
        (unsigned long long)st.st_size > (unsigned long long)max_bytes)
        return -1;
    if (file_size)
        *file_size = (size_t)st.st_size;
    return 1;
}

int ps3scale_ps2_config_store_stage(const char *path, const void *data,
                                    size_t length, size_t max_bytes)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset = 0;
    int descriptor;
    int created = 0;

    if (!path || !path[0] || !data || length == 0 ||
        max_bytes == 0 || length > max_bytes)
        return -1;

    if (ps3scale_ps2_config_store_probe(path, max_bytes, NULL) != 0)
        return -2;

    descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0)
        return -3;
    created = 1;

    while (offset < length) {
        ssize_t count = write(descriptor, bytes + offset, length - offset);

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

    {
        size_t staged_size = 0;
        if (ps3scale_ps2_config_store_probe(path, max_bytes,
                                            &staged_size) != 1 ||
            staged_size != length)
            goto fail_unlink;
    }

    return 0;

fail:
    (void)close(descriptor);
    descriptor = -1;
fail_unlink:
    if (created)
        (void)unlink(path);
    return -4;
}

int ps3scale_ps2_config_store_is_generated(const char *path,
                                           size_t max_bytes)
{
    FILE *file;
    unsigned char *data;
    size_t file_size = 0;
    int state;
    int valid;

    state = ps3scale_ps2_config_store_probe(path, max_bytes, &file_size);
    if (state != 1)
        return state;
    if (file_size == 0 || file_size > max_bytes)
        return -1;

    data = (unsigned char *)malloc(file_size);
    if (!data)
        return -1;

    file = fopen(path, "rb");
    if (!file) {
        free(data);
        return -1;
    }

    valid = fread(data, 1, file_size, file) == file_size;
    if (fclose(file) != 0)
        valid = 0;
    if (!valid) {
        free(data);
        return -1;
    }

    valid = ps3scale_ps2_config_validate_generated(data, file_size) == 0;
    free(data);
    return valid ? 1 : 0;
}

static int files_equal(const char *left, const char *right, size_t max_bytes)
{
    FILE *a = NULL;
    FILE *b = NULL;
    unsigned char abuf[COPY_CHUNK];
    unsigned char bbuf[COPY_CHUNK];
    size_t left_size = 0;
    size_t right_size = 0;
    size_t total = 0;
    int result = -1;

    if (ps3scale_ps2_config_store_probe(left, max_bytes, &left_size) != 1 ||
        ps3scale_ps2_config_store_probe(right, max_bytes, &right_size) != 1)
        return -1;
    if (left_size != right_size)
        return 0;

    a = fopen(left, "rb");
    b = fopen(right, "rb");
    if (!a || !b)
        goto done;

    for (;;) {
        size_t na = fread(abuf, 1, sizeof(abuf), a);
        size_t nb = fread(bbuf, 1, sizeof(bbuf), b);

        if (na != nb || (na && memcmp(abuf, bbuf, na) != 0)) {
            result = 0;
            goto done;
        }
        total += na;
        if (total > max_bytes)
            goto done;
        if (na < sizeof(abuf)) {
            if (ferror(a) || ferror(b))
                goto done;
            result = total == left_size ? 1 : -1;
            goto done;
        }
    }

done:
    if (a && fclose(a) != 0)
        result = -1;
    if (b && fclose(b) != 0)
        result = -1;
    return result;
}

static int copy_bounded(const char *source, const char *destination,
                        size_t max_bytes)
{
    FILE *src = NULL;
    FILE *dst = NULL;
    unsigned char buffer[COPY_CHUNK];
    size_t expected = 0;
    size_t total = 0;
    int fd = -1;
    int created = 0;
    int ok = 0;

    if (ps3scale_ps2_config_store_probe(source, max_bytes, &expected) != 1 ||
        ps3scale_ps2_config_store_probe(destination, max_bytes, NULL) != 0)
        return -1;

    src = fopen(source, "rb");
    if (!src)
        goto done;
    fd = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        goto done;
    created = 1;
    dst = fdopen(fd, "wb");
    if (!dst)
        goto done;
    fd = -1;

    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), src);

        if (count) {
            if (count > max_bytes || total > max_bytes - count ||
                fwrite(buffer, 1, count, dst) != count)
                goto done;
            total += count;
        }
        if (count < sizeof(buffer)) {
            if (ferror(src))
                goto done;
            break;
        }
    }

    if (total != expected || fflush(dst) != 0)
        goto done;
    ok = 1;

done:
    if (src && fclose(src) != 0)
        ok = 0;
    if (dst && fclose(dst) != 0)
        ok = 0;
    else if (fd >= 0)
        close(fd);
    if (!ok && created)
        (void)unlink(destination);
    return ok ? 0 : -1;
}

int ps3scale_ps2_config_store_can_replace(const char *target,
                                          const char *backup,
                                          size_t max_bytes)
{
    int target_state =
        ps3scale_ps2_config_store_probe(target, max_bytes, NULL);

    if (target_state == 0)
        return 0;
    if (target_state != 1)
        return -1;
    if (ps3scale_ps2_config_store_probe(backup, max_bytes, NULL) != 1)
        return -1;
    return files_equal(target, backup, max_bytes) == 1 ? 1 : -1;
}

int ps3scale_ps2_config_store_backup(const char *target, const char *backup,
                                     const char *temporary, size_t max_bytes)
{
    int backup_state;

    if (!target || !backup || !temporary || !target[0] || !backup[0] ||
        !temporary[0] || strcmp(target, backup) == 0 ||
        strcmp(target, temporary) == 0 || strcmp(backup, temporary) == 0)
        return -1;
    if (ps3scale_ps2_config_store_probe(target, max_bytes, NULL) != 1)
        return -4;

    backup_state = ps3scale_ps2_config_store_probe(backup, max_bytes, NULL);
    if (backup_state == 1)
        return -2;
    if (backup_state != 0 ||
        ps3scale_ps2_config_store_probe(temporary, max_bytes, NULL) != 0)
        return -3;

    if (copy_bounded(target, temporary, max_bytes) != 0)
        return -5;
    if (files_equal(target, temporary, max_bytes) != 1) {
        (void)unlink(temporary);
        return -5;
    }
    if (ps3scale_ps2_config_store_probe(backup, max_bytes, NULL) != 0 ||
        rename(temporary, backup) != 0) {
        (void)unlink(temporary);
        return -5;
    }
    return 0;
}

int ps3scale_ps2_config_store_restore(const char *target, const char *backup,
                                      const char *rollback_path,
                                      const char *temporary, size_t max_bytes)
{
    int target_state;

    if (!target || !backup || !rollback_path || !temporary ||
        !target[0] || !backup[0] || !rollback_path[0] || !temporary[0] ||
        strcmp(target, backup) == 0 || strcmp(target, rollback_path) == 0 ||
        strcmp(target, temporary) == 0 || strcmp(backup, rollback_path) == 0 ||
        strcmp(backup, temporary) == 0 ||
        strcmp(rollback_path, temporary) == 0)
        return -1;
    if (ps3scale_ps2_config_store_probe(backup, max_bytes, NULL) != 1)
        return -2;

    target_state = ps3scale_ps2_config_store_probe(target, max_bytes, NULL);
    if (target_state < 0)
        return -4;
    if (target_state == 1 && files_equal(target, backup, max_bytes) == 1)
        return 1;
    if (ps3scale_ps2_config_store_probe(temporary, max_bytes, NULL) != 0)
        return -3;
    if (target_state == 1) {
        int rollback_state =
            ps3scale_ps2_config_store_probe(rollback_path, max_bytes, NULL);
        if (rollback_state == 1 &&
            files_equal(rollback_path, backup, max_bytes) == 1) {
            /* The saved backup already contains these exact bytes. */
            if (unlink(rollback_path) != 0)
                return -3;
            rollback_state = 0;
        }
        if (rollback_state != 0)
            return -3;
    }

    if (copy_bounded(backup, temporary, max_bytes) != 0 ||
        files_equal(backup, temporary, max_bytes) != 1) {
        (void)unlink(temporary);
        return -5;
    }
    if (target_state == 1 && rename(target, rollback_path) != 0) {
        (void)unlink(temporary);
        return -5;
    }
    if (rename(temporary, target) != 0) {
        if (target_state == 1)
            (void)rename(rollback_path, target);
        (void)unlink(temporary);
        return -5;
    }
    return 0;
}

int ps3scale_ps2_config_store_install(const char *temporary,
                                      const char *target,
                                      const char *backup,
                                      const char *rollback_path,
                                      size_t max_bytes)
{
    int target_state;
    int rollback_state;

    if (!temporary || !target || !backup || !rollback_path ||
        !temporary[0] || !target[0] || !backup[0] || !rollback_path[0] ||
        strcmp(temporary, target) == 0 || strcmp(temporary, backup) == 0 ||
        strcmp(temporary, rollback_path) == 0 ||
        strcmp(target, backup) == 0 || strcmp(target, rollback_path) == 0 ||
        strcmp(backup, rollback_path) == 0)
        return -1;
    if (ps3scale_ps2_config_store_probe(temporary, max_bytes, NULL) != 1)
        return -4;

    target_state = ps3scale_ps2_config_store_probe(target, max_bytes, NULL);
    if (target_state < 0)
        return -4;

    if (target_state == 1) {
        if (ps3scale_ps2_config_store_can_replace(
                target, backup, max_bytes) != 1)
            return -2;

        rollback_state =
            ps3scale_ps2_config_store_probe(rollback_path, max_bytes, NULL);
        if (rollback_state < 0)
            return -3;

        if (rollback_state == 0) {
            /* Preserve the current file before attempting the replacement. */
            if (rename(target, rollback_path) != 0)
                return -4;
            if (rename(temporary, target) != 0) {
                (void)rename(rollback_path, target);
                return -4;
            }
            return 0;
        }

        /*
         * A rollback snapshot already exists. The immutable backup is a
         * byte-identical copy of the current target, so retain both snapshots
         * and use one atomic rename to replace the target.
         */
        return rename(temporary, target) == 0 ? 0 : -4;
    }

    return rename(temporary, target) == 0 ? 0 : -4;
}
