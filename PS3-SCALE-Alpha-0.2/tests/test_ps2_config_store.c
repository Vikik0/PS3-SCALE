#include "ps3scale/ps2_config_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_LIMIT 64u

static void write_file(const char *path, const char *contents)
{
    FILE *fp = fopen(path, "wb");
    size_t length = strlen(contents);

    assert(fp != NULL);
    assert(fwrite(contents, 1, length, fp) == length);
    assert(fclose(fp) == 0);
}

static void expect_file(const char *path, const char *expected)
{
    char buffer[128];
    FILE *fp = fopen(path, "rb");
    size_t length = strlen(expected);

    assert(fp != NULL);
    assert(length < sizeof(buffer));
    assert(fread(buffer, 1, sizeof(buffer), fp) == length);
    assert(memcmp(buffer, expected, length) == 0);
    assert(fclose(fp) == 0);
}

int main(void)
{
    char root[128];
    char target[192], backup[192], backup_temp[192];
    char rollback[192], restore_temp[192], download_temp[192];
    char target2[192], backup2[192], rollback2[192], download2[192];
    char oversized[192], oversized_backup[192], oversized_temp[192];
    char large[TEST_LIMIT + 2u];
    int written;

    written = snprintf(root, sizeof(root), "/tmp/ps3scale_ps2_store_%ld",
                       (long)getpid());
    assert(written > 0 && (size_t)written < sizeof(root));
    assert(mkdir(root, 0700) == 0);

#define MAKE_PATH(destination, suffix) do { \
    written = snprintf((destination), sizeof(destination), "%s/%s", root, (suffix)); \
    assert(written > 0 && (size_t)written < sizeof(destination)); \
} while (0)

    MAKE_PATH(target, "game.iso.CONFIG");
    MAKE_PATH(backup, "game.iso.CONFIG.bak");
    MAKE_PATH(backup_temp, "game.iso.CONFIG.backup.tmp");
    MAKE_PATH(rollback, "game.iso.CONFIG.rollback");
    MAKE_PATH(restore_temp, "game.iso.CONFIG.restore.tmp");
    MAKE_PATH(download_temp, "game.iso.CONFIG.download");
    MAKE_PATH(target2, "new.iso.CONFIG");
    MAKE_PATH(backup2, "new.iso.CONFIG.bak");
    MAKE_PATH(rollback2, "new.iso.CONFIG.rollback");
    MAKE_PATH(download2, "new.iso.CONFIG.download");
    MAKE_PATH(oversized, "large.CONFIG");
    MAKE_PATH(oversized_backup, "large.CONFIG.bak");
    MAKE_PATH(oversized_temp, "large.CONFIG.tmp");

    write_file(target, "original-config");
    assert(ps3scale_ps2_config_store_probe(target, TEST_LIMIT, NULL) == 1);
    assert(ps3scale_ps2_config_store_backup(
        target, backup, backup_temp, TEST_LIMIT) == 0);
    expect_file(backup, "original-config");
    assert(ps3scale_ps2_config_store_can_replace(
        target, backup, TEST_LIMIT) == 1);
    assert(ps3scale_ps2_config_store_backup(
        target, backup, backup_temp, TEST_LIMIT) == -2);

    write_file(target, "changed-config");
    assert(ps3scale_ps2_config_store_can_replace(
        target, backup, TEST_LIMIT) == -1);
    write_file(download_temp, "downloaded-config");
    assert(ps3scale_ps2_config_store_install(
        download_temp, target, backup, rollback, TEST_LIMIT) == -2);
    expect_file(target, "changed-config");
    expect_file(download_temp, "downloaded-config");

    assert(ps3scale_ps2_config_store_restore(
        target, backup, rollback, restore_temp, TEST_LIMIT) == 0);
    expect_file(target, "original-config");
    expect_file(rollback, "changed-config");
    assert(ps3scale_ps2_config_store_restore(
        target, backup, rollback, restore_temp, TEST_LIMIT) == 1);
    assert(unlink(rollback) == 0);

    assert(ps3scale_ps2_config_store_can_replace(
        target, backup, TEST_LIMIT) == 1);
    write_file(download_temp, "downloaded-config");
    assert(ps3scale_ps2_config_store_install(
        download_temp, target, backup, rollback, TEST_LIMIT) == 0);
    expect_file(target, "downloaded-config");
    expect_file(rollback, "original-config");
    assert(ps3scale_ps2_config_store_restore(
        target, backup, rollback, restore_temp, TEST_LIMIT) == 0);
    expect_file(target, "original-config");
    expect_file(rollback, "downloaded-config");
    assert(ps3scale_ps2_config_store_restore(
        target, backup, rollback, restore_temp, TEST_LIMIT) == 1);

    write_file(download_temp, "second-download");
    assert(ps3scale_ps2_config_store_install(
        download_temp, target, backup, rollback, TEST_LIMIT) == 0);
    expect_file(target, "second-download");
    expect_file(rollback, "downloaded-config");
    assert(ps3scale_ps2_config_store_restore(
        target, backup, rollback, restore_temp, TEST_LIMIT) == -3);

    write_file(download2, "new-title-config");
    assert(ps3scale_ps2_config_store_install(
        download2, target2, backup2, rollback2, TEST_LIMIT) == 0);
    expect_file(target2, "new-title-config");

    memset(large, 'X', sizeof(large));
    {
        FILE *fp = fopen(oversized, "wb");
        assert(fp != NULL);
        assert(fwrite(large, 1, sizeof(large), fp) == sizeof(large));
        assert(fclose(fp) == 0);
    }
    assert(ps3scale_ps2_config_store_probe(
        oversized, TEST_LIMIT, NULL) == -1);
    assert(ps3scale_ps2_config_store_backup(
        oversized, oversized_backup, oversized_temp, TEST_LIMIT) == -4);

    assert(unlink(target) == 0);
    assert(unlink(backup) == 0);
    assert(unlink(rollback) == 0);
    assert(unlink(target2) == 0);
    assert(unlink(oversized) == 0);
    assert(rmdir(root) == 0);

    puts("PS3 SCALE PS2 CONFIG backup/restore tests: PASS");
    return 0;
}
