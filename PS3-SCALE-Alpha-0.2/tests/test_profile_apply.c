#include "ps3scale/profile_apply.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/ps3scale-profile-apply-test"
#define TEST_SCRIPT_DIR TEST_ROOT "/wm_ingame"
#define TEST_SCRIPT_PATH TEST_SCRIPT_DIR "/BLUS30464.bat"

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

static int read_text(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t used;
    if (!file)
        return -1;
    used = fread(buffer, 1, capacity - 1, file);
    buffer[used] = '\0';
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

static void cleanup(void)
{
    (void)unlink(TEST_SCRIPT_PATH);
    (void)unlink(TEST_SCRIPT_PATH ".part");
    (void)rmdir(TEST_SCRIPT_DIR);
    (void)rmdir(TEST_ROOT);
}

int main(void)
{
    ps3scale_profile_t profile;
    char script[2048];
    size_t length;

    cleanup();
    if (mkdir(TEST_ROOT, 0777) != 0 && errno != EEXIST)
        return 1;

    memset(&profile, 0, sizeof(profile));
    strcpy(profile.title_id, "BLUS30464");
    strcpy(profile.app_ver, "01.05");
    strcpy(profile.title, "Skate 3");
    strcpy(profile.effect, "60 FPS");
    strcpy(profile.trust, "B_COMMUNITY");
    strcpy(profile.patches, "94713A,06,00");
    strcpy(profile.source_url, "https://github.com/example/profiles");
    profile.patch_count = 1;

    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == 0);
    assert(ps3scale_profile_script_install(&profile) == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == 1);

    /* A matching header is not enough: any edited script body is stale. */
    assert(write_text(TEST_SCRIPT_PATH,
        "# PS3 SCALE AUTO PROFILE V2 TITLE_ID=BLUS30464 APP_VER=01.05 PATCHES=94713A,06,00\n"
        "# MANUAL EDIT: body differs from the generated script\n") == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == -2);
    assert(ps3scale_profile_script_install(&profile) == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == 1);

    assert(ps3scale_profile_script_state(profile.title_id, "01.06",
                                         profile.patches) == -2);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         NULL) == -2);
    assert(ps3scale_profile_script_install(&profile) == 1);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         "947130,06,00") == -2);

    /* Address alignment must not widen the search to neighboring bytes. */
    strcpy(profile.patches, "947130,06,00");
    assert(ps3scale_profile_script_install(&profile) == 0);
    assert(read_text(TEST_SCRIPT_PATH, script, sizeof(script)) == 0);
    assert(strstr(script,
        "/patch.ps3?addr=00947130&stop=00947150&step=64&find=06&rep=1&val=00") != NULL);
    strcpy(profile.patches, "94713F,06,00");
    assert(ps3scale_profile_script_install(&profile) == 0);
    assert(read_text(TEST_SCRIPT_PATH, script, sizeof(script)) == 0);
    assert(strstr(script,
        "/patch.ps3?addr=0094713F&stop=0094715F&step=64&find=06&rep=1&val=00") != NULL);
    strcpy(profile.patches, "94713A,06,00");
    assert(ps3scale_profile_script_install(&profile) == 0);

    assert(read_text(TEST_SCRIPT_PATH, script, sizeof(script)) == 0);
    assert(strstr(script,
        "# PS3 SCALE AUTO PROFILE V2 TITLE_ID=BLUS30464 APP_VER=01.05 "
        "PATCHES=94713A,06,00") != NULL);
    assert(strstr(script,
        "/patch.ps3?addr=0094713A&stop=0094715A&step=64&find=06&rep=1&val=00") != NULL);
    assert(strstr(script, "del " TEST_SCRIPT_PATH) != NULL);
    assert(strstr(script, "end if\nend if\n") != NULL);
    assert(strstr(script, "popup") == NULL);
    assert(strstr(script, "beep") == NULL);
    length = strlen(script);
    assert(length >= 2 && script[length - 1] == '\n' &&
           script[length - 2] == '\n');

    /* A previous V1 script is owned but stale and can be safely removed. */
    assert(write_text(TEST_SCRIPT_PATH,
        "# PS3 SCALE AUTO PROFILE V1 TITLE_ID=BLUS30464 APP_VER=01.05\n") == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == -2);
    assert(ps3scale_profile_script_remove(profile.title_id) == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == 0);

    /* A script that is not owned by PS3 SCALE must never be overwritten/deleted. */
    assert(write_text(TEST_SCRIPT_PATH, "# USER WEBMAN SCRIPT\n") == 0);
    assert(ps3scale_profile_script_state(profile.title_id, profile.app_ver,
                                         profile.patches) == -1);
    assert(ps3scale_profile_script_install(&profile) == -2);
    assert(ps3scale_profile_script_remove(profile.title_id) == -2);
    assert(read_text(TEST_SCRIPT_PATH, script, sizeof(script)) == 0);
    assert(strcmp(script, "# USER WEBMAN SCRIPT\n") == 0);

    /* Multi-write, non-community and unsafe source entries are refused. */
    profile.patch_count = 2;
    assert(ps3scale_profile_script_install(&profile) == -3);
    profile.patch_count = 1;
    strcpy(profile.trust, "C_UNVERIFIED");
    assert(ps3scale_profile_script_install(&profile) == -1);
    strcpy(profile.trust, "B_COMMUNITY");
    strcpy(profile.source_url, "https://example.invalid/\nbeep1");
    assert(ps3scale_profile_script_install(&profile) == -1);

    cleanup();
    puts("PS3 SCALE one-launch profile script tests: PASS");
    return 0;
}
