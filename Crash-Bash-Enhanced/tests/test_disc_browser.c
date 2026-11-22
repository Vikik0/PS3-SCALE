#define _POSIX_C_SOURCE 200809L

#include "disc_browser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

int main(void)
{
    char root_template[] = "/tmp/cb-disc-browser-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[CB_DISC_PATH_MAX];
    const char *roots[2];
    CBGameDisc discs[CB_DISC_MAX];
    size_t count = 0;

    assert(root != NULL);
    snprintf(path, sizeof(path), "%s/Crash Bash.cue", root);
    make_file(path, "FILE \"Crash Bash.bin\" BINARY\n");
    snprintf(path, sizeof(path), "%s/Second Game.CUE", root);
    make_file(path, "FILE \"Second Game.bin\" BINARY\n");
    snprintf(path, sizeof(path), "%s/ignore.bin", root);
    make_file(path, "not a cue");
    snprintf(path, sizeof(path), "%s/fake.cue", root);
    assert(mkdir(path, 0700) == 0);

    roots[0] = root;
    roots[1] = root;
    assert(cb_disc_scan_roots(discs, CB_DISC_MAX, &count, roots, 2) == 0);
    assert(count == 2);
    assert(strcmp(discs[0].name, "Crash Bash") == 0 ||
           strcmp(discs[1].name, "Crash Bash") == 0);
    assert(strcmp(discs[0].name, "Second Game") == 0 ||
           strcmp(discs[1].name, "Second Game") == 0);

    count = 0;
    assert(cb_disc_scan_roots(discs, 1, &count, roots, 1) == 0);
    assert(count == 1);

    assert(cb_disc_scan_roots(discs, CB_DISC_MAX, &count, NULL, 0) == 0);
    assert(count == 0);
    assert(cb_disc_scan_roots(NULL, CB_DISC_MAX, &count, roots, 1) == -1);

    snprintf(path, sizeof(path), "%s/Crash Bash.cue", root);
    assert(unlink(path) == 0);
    snprintf(path, sizeof(path), "%s/Second Game.CUE", root);
    assert(unlink(path) == 0);
    snprintf(path, sizeof(path), "%s/ignore.bin", root);
    assert(unlink(path) == 0);
    snprintf(path, sizeof(path), "%s/fake.cue", root);
    assert(rmdir(path) == 0);
    assert(rmdir(root) == 0);

    puts("Disc browser tests passed.");
    return 0;
}
