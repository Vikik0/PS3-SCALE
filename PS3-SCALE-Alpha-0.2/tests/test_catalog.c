#include "ps3scale/catalog.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int write_catalog(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    size_t length;
    if (!file) return -1;
    length = strlen(contents);
    if (fwrite(contents, 1, length, file) != length) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

int main(void)
{
    static const char valid[] =
        "# PS3 SCALE catalogue test\n"
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "94713A,06,00\tExact version only.\t"
        "https://github.com/example/profiles\n"
        "BCUS98174\t01.11\tThe Last of Us\t60 FPS\tB_COMMUNITY\t"
        "1571A6F,02,01;1571A70,04,05\tTwo writes.\t"
        "https://github.com/example/profiles\n"
        "BLUS30330\t01.02\tBrutal Legend\tManual only\tB_COMMUNITY_MANUAL\t"
        "C479F,02,01\tReapply required in the main menu.\t"
        "https://github.com/example/profiles\n";
    static const char invalid[] =
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "94713A,06,GG\tBad replacement byte.\t"
        "https://github.com/example/profiles\n";
    static const char invalid_zero_address[] =
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "0,06,00\tZero address.\t"
        "https://github.com/example/profiles\n";
    static const char invalid_noop[] =
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "94713A,06,06\tNo-op patch.\t"
        "https://github.com/example/profiles\n";
    static const char invalid_source_url[] =
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "94713A,06,00\tControl character in source.\t"
        "https://example.com/\001bad\n";
    static const char invalid_high_address[] =
        "BLUS30464\t01.05\tSkate 3\t60 FPS\tB_COMMUNITY\t"
        "FFFFFFE0,06,00\tAddress overflows safe search window.\t"
        "https://github.com/example/profiles\n";
    const char *path = "/tmp/ps3scale_catalog_test.tsv";
    ps3scale_catalog_t catalog;
    ps3scale_game_list_t games;
    ps3scale_game_t game;

    memset(&catalog, 0, sizeof(catalog));
    if (write_catalog(path, valid) != 0)
        return 1;
    if (ps3scale_catalog_load(path, &catalog) != 3)
        return 2;
    if (catalog.count != 3 || catalog.items[0].patch_count != 1 ||
        catalog.items[1].patch_count != 2 ||
        strcmp(catalog.items[2].trust, "B_COMMUNITY_MANUAL") != 0)
        return 3;
    if (!ps3scale_catalog_match(&catalog, "BLUS30464", "01.05"))
        return 4;
    if (ps3scale_catalog_match(&catalog, "BLUS30464", "01.06"))
        return 5;
    if (ps3scale_catalog_match(&catalog, "BLUS30464", ""))
        return 6;

    ps3scale_game_list_init(&games);
    memset(&game, 0, sizeof(game));
    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.title_id, "BLUS30464");
    strcpy(game.app_ver, "01.05");
    strcpy(game.title, "Skate 3");
    if (ps3scale_game_list_add(&games, &game) != 1)
        return 7;
    game.kind = PS3SCALE_GAME_PS2_ISO;
    strcpy(game.title_id, "BCUS98174");
    strcpy(game.app_ver, "01.11");
    if (ps3scale_game_list_add(&games, &game) != 0)
        return 8;
    if (ps3scale_catalog_count_matches(&catalog, &games) != 1)
        return 9;

    if (write_catalog(path, invalid) != 0)
        return 10;
    if (ps3scale_catalog_load(path, &catalog) >= 0)
        return 11;
    if (catalog.count != 3) /* failed loads do not replace the last good cache */
        return 12;

    if (write_catalog(path, invalid_zero_address) != 0)
        return 13;
    if (ps3scale_catalog_load(path, &catalog) >= 0)
        return 14;
    if (catalog.count != 3)
        return 15;

    if (write_catalog(path, invalid_noop) != 0)
        return 16;
    if (ps3scale_catalog_load(path, &catalog) >= 0)
        return 17;
    if (catalog.count != 3)
        return 18;

    if (write_catalog(path, invalid_high_address) != 0)
        return 19;
    if (ps3scale_catalog_load(path, &catalog) >= 0)
        return 20;
    if (catalog.count != 3)
        return 21;

    if (write_catalog(path, invalid_source_url) != 0)
        return 22;
    if (ps3scale_catalog_load(path, &catalog) >= 0)
        return 23;
    if (catalog.count != 3)
        return 24;

    unlink(path);
    puts("PS3 SCALE online catalog tests: PASS");
    return 0;
}
