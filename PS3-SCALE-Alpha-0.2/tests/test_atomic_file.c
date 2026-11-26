#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "ps3scale/atomic_file.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/ps3scale-atomic-file-test"
#define TEST_TARGET TEST_ROOT "/settings.cfg"
#define TEST_VICTIM TEST_ROOT "/user-owned.txt"

static int write_text(const char *path, const char *value)
{
    FILE *file = fopen(path, "wb");
    size_t length = strlen(value);
    if (!file)
        return -1;
    if (fwrite(value, 1, length, file) != length) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

static size_t read_text(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t used;
    assert(file != NULL);
    used = fread(buffer, 1, capacity - 1, file);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    buffer[used] = '\0';
    return used;
}

static size_t count_temporary_files(void)
{
    DIR *directory = opendir(TEST_ROOT);
    struct dirent *entry;
    size_t count = 0;
    assert(directory != NULL);
    while ((entry = readdir(directory)) != NULL) {
        if (strstr(entry->d_name, ".ps3scale.") != NULL)
            count++;
    }
    assert(closedir(directory) == 0);
    return count;
}

static void cleanup(void)
{
    DIR *directory;
    struct dirent *entry;
    (void)unlink(TEST_TARGET);
    (void)unlink(TEST_VICTIM);
    directory = opendir(TEST_ROOT);
    if (directory) {
        while ((entry = readdir(directory)) != NULL) {
            char path[512];
            int length;
            if (!strstr(entry->d_name, ".ps3scale."))
                continue;
            length = snprintf(path, sizeof(path), "%s/%s",
                              TEST_ROOT, entry->d_name);
            if (length > 0 && (size_t)length < sizeof(path))
                (void)unlink(path);
        }
        (void)closedir(directory);
    }
    (void)rmdir(TEST_ROOT);
}

int main(void)
{
    char buffer[128];
    struct stat st;

    cleanup();
    assert(mkdir(TEST_ROOT, 0777) == 0);
    assert(write_text(TEST_TARGET, "old settings") == 0);

    assert(ps3scale_atomic_file_write(TEST_TARGET,
                                      "new settings", 12) == 0);
    assert(read_text(TEST_TARGET, buffer, sizeof(buffer)) == 12);
    assert(strcmp(buffer, "new settings") == 0);

    /* Replacing a symlink replaces the link itself, never its target. */
    assert(write_text(TEST_VICTIM, "user data stays here") == 0);
    assert(unlink(TEST_TARGET) == 0);
    assert(symlink(TEST_VICTIM, TEST_TARGET) == 0);
    assert(ps3scale_atomic_file_write(TEST_TARGET, "replacement", 11) == 0);
    assert(read_text(TEST_VICTIM, buffer, sizeof(buffer)) == 20);
    assert(strcmp(buffer, "user data stays here") == 0);
    assert(read_text(TEST_TARGET, buffer, sizeof(buffer)) == 11);
    assert(strcmp(buffer, "replacement") == 0);
    assert(lstat(TEST_TARGET, &st) == 0 && S_ISREG(st.st_mode));

    /* An empty favourites list is a valid atomic replacement. */
    assert(ps3scale_atomic_file_write(TEST_TARGET, NULL, 0) == 0);
    assert(read_text(TEST_TARGET, buffer, sizeof(buffer)) == 0);

    /* Invalid destinations fail without leaving temporary files behind. */
    assert(ps3scale_atomic_file_write(NULL, "x", 1) == -1);
    assert(ps3scale_atomic_file_write(TEST_TARGET, NULL, 1) == -1);
    assert(ps3scale_atomic_file_write(TEST_ROOT, "x", 1) == -1);
    assert(count_temporary_files() == 0);

    cleanup();
    puts("PS3 SCALE atomic settings/favorites file tests: PASS");
    return 0;
}
