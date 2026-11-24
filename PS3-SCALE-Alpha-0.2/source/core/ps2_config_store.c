#include "ps3scale/ps2_config_store.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int regular_file_size(const char *path, size_t max_bytes,
                             size_t *size_out)
{
    struct stat st;

    if (size_out)
        *size_out = 0;
    if (!path || !path[0] || !size_out)
        return PS3SCALE_PS2_STORE_ERR_INVALID;
    if (lstat(path, &st) != 0)
        return errno == ENOENT ? PS3SCALE_PS2_STORE_ERR_NOT_FOUND :
                                PS3SCALE_PS2_STORE_ERR_IO;
    if (!S_ISREG(st.st_mode) || st.st_size < 0)
        return PS3SCALE_PS2_STORE_ERR_INVALID;
    if ((unsigned long long)st.st_size > (unsigned long long)max_bytes)
        return PS3SCALE_PS2_STORE_ERR_TOO_LARGE;
    *size_out = (size_t)st.st_size;
    return PS3SCALE_PS2_STORE_OK;
}

static uint32_t path_hash(const char *value)
{
    uint32_t hash = 2166136261u;
    const unsigned char *p = (const unsigned char *)value;

    while (*p) {
        hash ^= (uint32_t)*p++;
        hash *= 16777619u;
    }
    return hash;
}

int ps3scale_ps2_config_store_make_paths(
    char *backup_path, size_t backup_capacity,
    char *rollback_path, size_t rollback_capacity,
    const char *backup_directory, const char *title_id,
    const char *target_path)
{
    char safe_id[32];
    size_t used = 0;
    size_t i;
    uint32_t hash;
    int a, b;

    if (!backup_path || !rollback_path || !backup_directory ||
        !backup_directory[0] || !target_path || !target_path[0] ||
        !backup_capacity || !rollback_capacity)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    if (title_id && title_id[0]) {
        for (i = 0; title_id[i] && used + 1 < sizeof(safe_id); i++) {
            unsigned char ch = (unsigned char)title_id[i];
            safe_id[used++] = (char)(isalnum(ch) || ch == '_' ||
                                     ch == '-' || ch == '.' ? ch : '_');
        }
    }
    if (!used) {
        memcpy(safe_id, "UNKNOWN", 8);
        used = 7;
    }
    safe_id[used] = '\0';
    hash = path_hash(target_path);

    a = snprintf(backup_path, backup_capacity,
                 "%s/%s_%08lx.original.bak", backup_directory, safe_id,
                 (unsigned long)hash);
    b = snprintf(rollback_path, rollback_capacity,
                 "%s/%s_%08lx.rollback.bak", backup_directory, safe_id,
                 (unsigned long)hash);
    if (a < 0 || b < 0 || (size_t)a >= backup_capacity ||
        (size_t)b >= rollback_capacity) {
        backup_path[0] = '\0';
        rollback_path[0] = '\0';
        return PS3SCALE_PS2_STORE_ERR_INVALID;
    }
    return PS3SCALE_PS2_STORE_OK;
}

int ps3scale_ps2_config_store_file_matches(
    const char *left_path, const char *right_path, size_t max_bytes)
{
    FILE *left = NULL;
    FILE *right = NULL;
    unsigned char left_buffer[2048];
    unsigned char right_buffer[2048];
    size_t left_size, right_size;
    int result = -1;

    if (regular_file_size(left_path, max_bytes, &left_size) !=
            PS3SCALE_PS2_STORE_OK ||
        regular_file_size(right_path, max_bytes, &right_size) !=
            PS3SCALE_PS2_STORE_OK)
        return -1;
    if (left_size != right_size)
        return 0;

    left = fopen(left_path, "rb");
    right = fopen(right_path, "rb");
    if (!left || !right)
        goto cleanup;

    for (;;) {
        size_t a = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t b = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (a != b || (a && memcmp(left_buffer, right_buffer, a) != 0)) {
            result = 0;
            break;
        }
        if (a < sizeof(left_buffer)) {
            if (ferror(left) || ferror(right))
                result = -1;
            else
                result = feof(left) && feof(right) ? 1 : 0;
            break;
        }
    }

cleanup:
    if (left)
        fclose(left);
    if (right)
        fclose(right);
    return result;
}

static int copy_file_to_exclusive_temp(const char *source_path,
                                       const char *destination_path,
                                       const char *temporary_suffix,
                                       size_t max_bytes,
                                       char *temporary_path,
                                       size_t temporary_capacity)
{
    FILE *source = NULL;
    FILE *destination = NULL;
    struct stat st;
    int fd = -1;
    int written;
    int result = PS3SCALE_PS2_STORE_ERR_IO;
    unsigned char buffer[2048];

    if (!source_path || !destination_path || !temporary_suffix ||
        !temporary_path || !temporary_capacity)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    written = snprintf(temporary_path, temporary_capacity, "%s%s",
                       destination_path, temporary_suffix);
    if (written < 0 || (size_t)written >= temporary_capacity)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    {
        size_t source_size;
        int check = regular_file_size(source_path, max_bytes, &source_size);
        if (check != PS3SCALE_PS2_STORE_OK)
            return check;
    }

    if (lstat(temporary_path, &st) == 0)
        return PS3SCALE_PS2_STORE_ERR_CONFLICT;
    if (errno != ENOENT)
        return PS3SCALE_PS2_STORE_ERR_IO;

    source = fopen(source_path, "rb");
    if (!source)
        return PS3SCALE_PS2_STORE_ERR_IO;

    fd = open(temporary_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        goto cleanup;
    destination = fdopen(fd, "wb");
    if (!destination) {
        close(fd);
        fd = -1;
        goto cleanup;
    }
    fd = -1;

    for (;;) {
        size_t amount = fread(buffer, 1, sizeof(buffer), source);
        if (amount && fwrite(buffer, 1, amount, destination) != amount)
            goto cleanup;
        if (amount < sizeof(buffer)) {
            if (ferror(source))
                goto cleanup;
            break;
        }
    }

    if (fflush(destination) != 0)
        goto cleanup;
    if (fclose(destination) != 0) {
        destination = NULL;
        goto cleanup;
    }
    destination = NULL;
    fclose(source);
    source = NULL;

    if (ps3scale_ps2_config_store_file_matches(
            source_path, temporary_path, max_bytes) != 1) {
        result = PS3SCALE_PS2_STORE_ERR_VERIFY;
        goto cleanup;
    }
    return PS3SCALE_PS2_STORE_OK;

cleanup:
    if (destination)
        fclose(destination);
    else if (fd >= 0)
        close(fd);
    if (source)
        fclose(source);
    unlink(temporary_path);
    return result;
}

static int copy_file_atomic(const char *source_path,
                            const char *destination_path,
                            const char *temporary_suffix,
                            size_t max_bytes)
{
    char temporary_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
    struct stat st;
    int result;

    result = copy_file_to_exclusive_temp(
        source_path, destination_path, temporary_suffix, max_bytes,
        temporary_path, sizeof(temporary_path));
    if (result != PS3SCALE_PS2_STORE_OK)
        return result;

    if (lstat(destination_path, &st) == 0) {
        unlink(temporary_path);
        return PS3SCALE_PS2_STORE_ERR_EXISTS;
    }
    if (errno != ENOENT) {
        unlink(temporary_path);
        return PS3SCALE_PS2_STORE_ERR_IO;
    }
    if (rename(temporary_path, destination_path) != 0) {
        unlink(temporary_path);
        return PS3SCALE_PS2_STORE_ERR_IO;
    }
    return ps3scale_ps2_config_store_file_matches(
        source_path, destination_path, max_bytes) == 1 ?
        PS3SCALE_PS2_STORE_OK : PS3SCALE_PS2_STORE_ERR_VERIFY;
}

int ps3scale_ps2_config_store_backup(
    const char *source_path, const char *backup_path, size_t max_bytes)
{
    struct stat st;
    int matches;

    if (!source_path || !backup_path || !source_path[0] ||
        !backup_path[0] || !max_bytes)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    {
        size_t source_size;
        int check = regular_file_size(source_path, max_bytes, &source_size);
        if (check != PS3SCALE_PS2_STORE_OK)
            return check;
    }

    if (lstat(backup_path, &st) == 0) {
        matches = ps3scale_ps2_config_store_file_matches(
            source_path, backup_path, max_bytes);
        return matches == 1 ? PS3SCALE_PS2_STORE_ALREADY_MATCHES :
               matches == 0 ? PS3SCALE_PS2_STORE_ERR_CONFLICT :
                              PS3SCALE_PS2_STORE_ERR_IO;
    }
    if (errno != ENOENT)
        return PS3SCALE_PS2_STORE_ERR_IO;

    return copy_file_atomic(source_path, backup_path,
                            ".ps3scale.tmp", max_bytes);
}

int ps3scale_ps2_config_store_restore(
    const char *backup_path, const char *target_path,
    const char *rollback_path, size_t max_bytes)
{
    struct stat st;
    size_t backup_size;
    int target_exists = 0;
    int result;

    if (!backup_path || !target_path || !rollback_path ||
        !backup_path[0] || !target_path[0] || !rollback_path[0] ||
        !max_bytes)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    result = regular_file_size(backup_path, max_bytes, &backup_size);
    if (result != PS3SCALE_PS2_STORE_OK)
        return result;

    if (lstat(target_path, &st) == 0) {
        size_t target_size;
        target_exists = 1;
        result = regular_file_size(target_path, max_bytes, &target_size);
        if (result != PS3SCALE_PS2_STORE_OK)
            return result;
        if (ps3scale_ps2_config_store_file_matches(
                backup_path, target_path, max_bytes) == 1)
            return PS3SCALE_PS2_STORE_ALREADY_MATCHES;
    } else if (errno != ENOENT) {
        return PS3SCALE_PS2_STORE_ERR_IO;
    }

    if (target_exists) {
        result = ps3scale_ps2_config_store_backup(
            target_path, rollback_path, max_bytes);
        if (result != PS3SCALE_PS2_STORE_OK &&
            result != PS3SCALE_PS2_STORE_ALREADY_MATCHES)
            return result;
    }

    {
        char temporary_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
        int written = snprintf(temporary_path, sizeof(temporary_path),
                               "%s.ps3scale.restore.tmp", target_path);
        FILE *source = NULL;
        FILE *destination = NULL;
        int fd = -1;
        unsigned char buffer[2048];

        if (written < 0 || (size_t)written >= sizeof(temporary_path))
            return PS3SCALE_PS2_STORE_ERR_INVALID;
        if (lstat(temporary_path, &st) == 0)
            return PS3SCALE_PS2_STORE_ERR_CONFLICT;
        if (errno != ENOENT)
            return PS3SCALE_PS2_STORE_ERR_IO;

        source = fopen(backup_path, "rb");
        if (!source)
            return PS3SCALE_PS2_STORE_ERR_IO;
        fd = open(temporary_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            fclose(source);
            return PS3SCALE_PS2_STORE_ERR_IO;
        }
        destination = fdopen(fd, "wb");
        if (!destination) {
            close(fd);
            fclose(source);
            unlink(temporary_path);
            return PS3SCALE_PS2_STORE_ERR_IO;
        }

        for (;;) {
            size_t amount = fread(buffer, 1, sizeof(buffer), source);
            if (amount && fwrite(buffer, 1, amount, destination) != amount) {
                fclose(source);
                fclose(destination);
                unlink(temporary_path);
                return PS3SCALE_PS2_STORE_ERR_IO;
            }
            if (amount < sizeof(buffer)) {
                if (ferror(source)) {
                    fclose(source);
                    fclose(destination);
                    unlink(temporary_path);
                    return PS3SCALE_PS2_STORE_ERR_IO;
                }
                break;
            }
        }
        if (fflush(destination) != 0) {
            fclose(source);
            fclose(destination);
            unlink(temporary_path);
            return PS3SCALE_PS2_STORE_ERR_IO;
        }
        fclose(source);
        if (fclose(destination) != 0) {
            unlink(temporary_path);
            return PS3SCALE_PS2_STORE_ERR_IO;
        }
        if (ps3scale_ps2_config_store_file_matches(
                backup_path, temporary_path, max_bytes) != 1) {
            unlink(temporary_path);
            return PS3SCALE_PS2_STORE_ERR_VERIFY;
        }
        if (rename(temporary_path, target_path) != 0) {
            unlink(temporary_path);
            return PS3SCALE_PS2_STORE_ERR_IO;
        }
    }

    return ps3scale_ps2_config_store_file_matches(
        backup_path, target_path, max_bytes) == 1 ?
        PS3SCALE_PS2_STORE_OK : PS3SCALE_PS2_STORE_ERR_VERIFY;
}

int ps3scale_ps2_config_store_install_download(
    const char *download_path, const char *target_path,
    const char *original_backup_path, const char *rollback_path,
    size_t max_bytes, int allow_replace)
{
    struct stat st;
    size_t download_size;
    int target_exists = 0;
    int result;

    if (!download_path || !target_path || !download_path[0] ||
        !target_path[0] || !max_bytes)
        return PS3SCALE_PS2_STORE_ERR_INVALID;

    result = regular_file_size(download_path, max_bytes, &download_size);
    if (result != PS3SCALE_PS2_STORE_OK)
        return result;

    if (lstat(target_path, &st) == 0) {
        size_t target_size;
        target_exists = 1;
        result = regular_file_size(target_path, max_bytes, &target_size);
        if (result != PS3SCALE_PS2_STORE_OK)
            return result;
    } else if (errno != ENOENT) {
        return PS3SCALE_PS2_STORE_ERR_IO;
    }

    if (target_exists) {
        if (!allow_replace)
            return PS3SCALE_PS2_STORE_ERR_EXISTS;
        if (!original_backup_path || !original_backup_path[0] ||
            ps3scale_ps2_config_store_file_matches(
                target_path, original_backup_path, max_bytes) != 1)
            return PS3SCALE_PS2_STORE_ERR_BACKUP_REQUIRED;

        /* The immutable original backup already contains these exact bytes.
         * Do not replace it with the downloaded file or overwrite a rollback
         * snapshot created by an earlier restore. */
        (void)rollback_path;
    }

    /* Download and target are on the same HDD; rename replaces atomically. */
    if (rename(download_path, target_path) != 0)
        return PS3SCALE_PS2_STORE_ERR_IO;

    {
        size_t installed_size;
        result = regular_file_size(target_path, max_bytes, &installed_size);
        if (result != PS3SCALE_PS2_STORE_OK || installed_size != download_size)
            return PS3SCALE_PS2_STORE_ERR_VERIFY;
    }
    return PS3SCALE_PS2_STORE_OK;
}
