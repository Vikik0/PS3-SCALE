#ifndef PS3SCALE_PS2_CONFIG_STORE_H
#define PS3SCALE_PS2_CONFIG_STORE_H

#include <stddef.h>

#define PS3SCALE_PS2_CONFIG_STORE_PATH_MAX 1024u

enum {
    PS3SCALE_PS2_STORE_OK = 0,
    PS3SCALE_PS2_STORE_ALREADY_MATCHES = 1,
    PS3SCALE_PS2_STORE_ERR_INVALID = -1,
    PS3SCALE_PS2_STORE_ERR_NOT_FOUND = -2,
    PS3SCALE_PS2_STORE_ERR_TOO_LARGE = -3,
    PS3SCALE_PS2_STORE_ERR_CONFLICT = -4,
    PS3SCALE_PS2_STORE_ERR_IO = -5,
    PS3SCALE_PS2_STORE_ERR_VERIFY = -6,
    PS3SCALE_PS2_STORE_ERR_EXISTS = -7,
    PS3SCALE_PS2_STORE_ERR_BACKUP_REQUIRED = -8
};

/*
 * Build deterministic, title-scoped backup paths. The target path is hashed
 * as well as the title ID so two installations of the same serial cannot
 * accidentally share a backup.
 */
int ps3scale_ps2_config_store_make_paths(
    char *backup_path, size_t backup_capacity,
    char *rollback_path, size_t rollback_capacity,
    const char *backup_directory, const char *title_id,
    const char *target_path);

/* Returns 1 for byte-identical files, 0 for different files, and -1 on I/O. */
int ps3scale_ps2_config_store_file_matches(
    const char *left_path, const char *right_path, size_t max_bytes);

/* Create a backup once. Existing identical backups are preserved; different
 * existing backups are never overwritten. */
int ps3scale_ps2_config_store_backup(
    const char *source_path, const char *backup_path, size_t max_bytes);

/* Restore a backup atomically. If a target exists, preserve it in rollback_path
 * before replacing it. Existing non-identical rollback files block the action. */
int ps3scale_ps2_config_store_restore(
    const char *backup_path, const char *target_path,
    const char *rollback_path, size_t max_bytes);

/* Install a validated downloaded file. Replacing an existing target requires
 * an original backup that still matches the current target; the current target
 * is also preserved in rollback_path before the atomic replacement. */
int ps3scale_ps2_config_store_install_download(
    const char *download_path, const char *target_path,
    const char *original_backup_path, const char *rollback_path,
    size_t max_bytes, int allow_replace);

#endif
