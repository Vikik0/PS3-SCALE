#include "ps3scale/ps2_config_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char *path, const char *value)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(value, 1, strlen(value), fp) == strlen(value));
    assert(fclose(fp) == 0);
}

static void expect_file(const char *path, const char *value)
{
    char buffer[128];
    FILE *fp = fopen(path, "rb");
    size_t amount;
    assert(fp != NULL);
    amount = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[amount] = '\0';
    assert(!ferror(fp));
    fclose(fp);
    assert(strcmp(buffer, value) == 0);
}

int main(void)
{
    const char *root = "/tmp/ps3scale_ps2_store_test";
    const char *target = "/tmp/ps3scale_ps2_store_test/game.iso.CONFIG";
    const char *download = "/tmp/ps3scale_ps2_store_test/download.tmp";
    const char *backup = "/tmp/ps3scale_ps2_store_test/SLUS_209.09.original.bak";
    const char *rollback = "/tmp/ps3scale_ps2_store_test/SLUS_209.09.rollback.bak";
    char backup_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
    char rollback_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
    char other_backup[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
    char other_rollback[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];

    (void)unlink(target);
    (void)unlink(download);
    (void)unlink(backup);
    (void)unlink(rollback);
    (void)rmdir(root);
    assert(mkdir(root, 0700) == 0);

    write_file(target, "original-config");
    assert(ps3scale_ps2_config_store_backup(
        target, backup, 16384u) == PS3SCALE_PS2_STORE_OK);
    expect_file(backup, "original-config");
    assert(ps3scale_ps2_config_store_backup(
        target, backup, 16384u) == PS3SCALE_PS2_STORE_ALREADY_MATCHES);

    write_file(target, "user-edited-config");
    assert(ps3scale_ps2_config_store_backup(
        target, backup, 16384u) == PS3SCALE_PS2_STORE_ERR_CONFLICT);
    expect_file(backup, "original-config");

    /* Restore preserves the current file in a separate rollback copy. */
    assert(ps3scale_ps2_config_store_restore(
        backup, target, rollback, 16384u) == PS3SCALE_PS2_STORE_OK);
    expect_file(target, "original-config");
    expect_file(rollback, "user-edited-config");

    /* A second restore is a no-op and does not replace the rollback copy. */
    assert(ps3scale_ps2_config_store_restore(
        backup, target, rollback, 16384u) ==
        PS3SCALE_PS2_STORE_ALREADY_MATCHES);
    expect_file(rollback, "user-edited-config");

    /* Replacement requires the exact original backup and leaves rollback intact. */
    write_file(download, "community-config-v2");
    assert(ps3scale_ps2_config_store_install_download(
        download, target, backup, 16384u, 1) ==
        PS3SCALE_PS2_STORE_OK);
    expect_file(target, "community-config-v2");
    expect_file(rollback, "user-edited-config");

    write_file(download, "replacement-without-consent");
    assert(ps3scale_ps2_config_store_install_download(
        download, target, NULL, 16384u, 0) ==
        PS3SCALE_PS2_STORE_ERR_EXISTS);
    expect_file(target, "community-config-v2");

    /* The immutable original backup must not be overwritten on later changes. */
    write_file(download, "community-config-v3");
    assert(ps3scale_ps2_config_store_install_download(
        download, target, backup, 16384u, 1) ==
        PS3SCALE_PS2_STORE_ERR_BACKUP_REQUIRED);
    expect_file(target, "community-config-v2");
    expect_file(backup, "original-config");

    /* A missing target can be installed without creating a backup. */
    assert(unlink(target) == 0);
    write_file(download, "first-install");
    assert(ps3scale_ps2_config_store_install_download(
        download, target, NULL, 16384u, 0) ==
        PS3SCALE_PS2_STORE_OK);
    expect_file(target, "first-install");

    assert(ps3scale_ps2_config_store_make_paths(
        backup_path, sizeof(backup_path),
        rollback_path, sizeof(rollback_path),
        root, "../SLUS/209.09", target) == PS3SCALE_PS2_STORE_OK);
    assert(strstr(backup_path, ".._SLUS_209.09_") != NULL);
    {
        const char *base = strrchr(backup_path, '/');
        assert(base != NULL);
        assert(strchr(base + 1, '/') == NULL);
    }
    assert(strstr(backup_path, ".original.bak") != NULL);
    assert(strstr(rollback_path, ".rollback.bak") != NULL);

    assert(ps3scale_ps2_config_store_make_paths(
        other_backup, sizeof(other_backup),
        other_rollback, sizeof(other_rollback),
        root, "../SLUS/209.09", "/different/location/game.iso.CONFIG") ==
        PS3SCALE_PS2_STORE_OK);
    assert(strcmp(backup_path, other_backup) != 0);
    assert(strcmp(rollback_path, other_rollback) != 0);

    (void)unlink(target);
    (void)unlink(download);
    (void)unlink(backup);
    (void)unlink(rollback);
    (void)rmdir(root);
    puts("PS3 SCALE PS2 CONFIG backup/restore tests: PASS");
    return 0;
}
