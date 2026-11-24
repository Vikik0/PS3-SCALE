#include "ps3scale/community.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int write_index(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    size_t length;
    if (!file)
        return -1;
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
        "# PS3 SCALE community index test\n"
        "BLUS30464\t01.05\tSkate 3\tHARDWARE_TESTED_FPS\t"
        "Nascar1243/PS3-FPS-Patches\tmain\t"
        "Skate 3 BLUS30464 v01.05.ncl\t31\tGRAPHICS_FPS\tUnlock FPS\t0000000000000000000000000000000000000000\n"
        "\t\tKirby Nightmare in Dream Land\tCOMMUNITY_CONFIRMED\t"
        "DoSpamu/RPCS3toArtemisPatches\tmaster\t"
        "PSXPlace Confirmed/Kirby Nightmare in Dream Land.ncl\t200\n"
        "RXKIR1131\t01.00\t\tCOMMUNITY_CONFIRMED\t"
        "DoSpamu/RPCS3toArtemisPatches\tmaster\t"
        "PSXPlace Confirmed/RXKIR1131 01.00.ncl\t100\n"
        "BLUS30253\t01.02\tSkate 2\tARTEMIS_DATABASE\t"
        "RobertoTorino/ArtemisPS3Offline\tmain\t"
        "codes/Skate 2 BLUS30253 01.02.ncl\t500\n"
        "BLES00016\t01.02\tCall Of Duty 3\tCOMMUNITY_UNVERIFIED\t"
        "DoSpamu/RPCS3toArtemisPatches\tmaster\t"
        "USERLIST/Call Of Duty 3 BLES00016 BLES00021 v01.02.ncl\t394\t"
        "COMMUNITY_CODE\tCommunity code; graphics effect not confirmed.\n"
        "BLES00517\t01.02\tJust Cause 2\tHARDWARE_TESTED_FPS\t"
        "Nascar1243/PS3-FPS-Patches\tmain\t"
        "Just Cause 2 BLES00517 BLUS30400 01.02.ncl\t440\t"
        "GRAPHICS_FPS\tUnlock FPS\t1111111111111111111111111111111111111111\n";
    static const char invalid[] =
        "BLUS30464\t01.05\tSkate 3\tUNKNOWN_SOURCE\t"
        "Nascar1243/PS3-FPS-Patches\tmain\t"
        "Skate 3 BLUS30464 v01.05.ncl\t31\n";
    static const char invalid_path[] =
        "BLUS30464\t01.05\tSkate 3\tHARDWARE_TESTED_FPS\t"
        "Nascar1243/PS3-FPS-Patches\tmain\t"
        "../outside/Skate 3.ncl\t31\n";

    const char *path = "/tmp/ps3scale_community_test.tsv";
    const char *hash_path = "/tmp/ps3scale_git_blob_test.ncl";
    ps3scale_community_index_t index;
    ps3scale_game_t game;

    memset(&index, 0, sizeof(index));
    if (write_index(hash_path, "hello\n") != 0)
        return 29;
    if (ps3scale_community_verify_git_blob_sha1(
            hash_path, 6, "ce013625030ba8dba906f756967f9e9ca394464a") != 1)
        return 30;
    if (write_index(hash_path, "hallo\n") != 0)
        return 31;
    if (ps3scale_community_verify_git_blob_sha1(
            hash_path, 6, "ce013625030ba8dba906f756967f9e9ca394464a") != 0)
        return 32;
    unlink(hash_path);
    if (write_index(path, valid) != 0)
        return 1;
    if (ps3scale_community_index_load(path, &index) != 6)
        return 2;
    if (index.count != 6)
        return 3;
    if (strlen(index.items[0].git_blob_sha1) != 40)
        return 28;

    memset(&game, 0, sizeof(game));
    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.title, "Skate 3");
    strcpy(game.title_id, "BLUS30464");
    strcpy(game.app_ver, "01.06");
    if (!ps3scale_community_file_matches_game(&index.items[0], &game))
        return 4;

    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.title, "Skate 3");
    strcpy(game.title_id, "BLUS30464");
    strcpy(game.app_ver, "01.05");
    if (!ps3scale_community_file_matches_exact_game(&index.items[0], &game))
        return 16;
    strcpy(game.app_ver, "01.06");
    if (ps3scale_community_file_matches_exact_game(&index.items[0], &game))
        return 17;
    strcpy(game.app_ver, "01.05");
    strcpy(game.title_id, "BLES30464");
    if (ps3scale_community_file_matches_exact_game(&index.items[0], &game))
        return 18;
    /* Same title and region are not sufficient for an install: the actual
       Title ID must be present in the indexed file identity/path metadata. */
    strcpy(game.title_id, "BLUS30465");
    if (ps3scale_community_file_matches_exact_game(&index.items[0], &game))
        return 24;
    if (strcmp(ps3scale_community_region_label("BLES00016"), "EU") != 0 ||
        strcmp(ps3scale_community_region_label("BLUS30012"), "US") != 0 ||
        strcmp(ps3scale_community_region_label("NPHA12345"), "ASIA") != 0 ||
        strcmp(ps3scale_community_region_label("NPKS12345"), "KOREA") != 0 ||
        strcmp(ps3scale_community_region_label("NPJA12345"), "JAPAN") != 0)
        return 19;

    {
        ps3scale_community_file_t cross_region = index.items[0];
        strcpy(game.title_id, "BLES30464");
        strcpy(game.app_ver, "01.05");
        strcpy(cross_region.path, "Skate 3 BLUS30464 v01.05.ncl");
        if (ps3scale_community_file_matches_exact_game(&cross_region, &game))
            return 27;
    }

    strcpy(game.title, "Just Cause 2");
    strcpy(game.title_id, "BLUS30400");
    strcpy(game.app_ver, "01.02");
    if (!ps3scale_community_file_matches_exact_game(&index.items[5], &game))
        return 23;

    strcpy(game.title, "Call of Duty: Modern Warfare 3");
    strcpy(game.title_id, "BLES01428");
    if (ps3scale_community_file_matches_game(&index.items[4], &game))
        return 20;

    game.title_id[0] = '\0';
    strcpy(game.title, "Kirby's Nightmare in Dream Land");
    if (!ps3scale_community_file_matches_game(&index.items[1], &game))
        return 5;

    strcpy(game.title, "Skate 3");
    if (ps3scale_community_file_matches_game(&index.items[3], &game))
        return 6;

    game.kind = PS3SCALE_GAME_PS3_HDD;
    strcpy(game.title, "Kirby");
    strcpy(game.title_id, "RXKIR1131");
    if (!ps3scale_community_file_matches_game(&index.items[2], &game))
        return 15;

    game.kind = PS3SCALE_GAME_PS2_ISO;
    strcpy(game.title, "Kirby's Nightmare in Dream Land");
    if (ps3scale_community_file_matches_game(&index.items[1], &game))
        return 7;

    if (strcmp(ps3scale_community_source_label("HARDWARE_TESTED_FPS"),
               "FPS PATCH REPOSITORY") != 0)
        return 8;
    if (strcmp(index.items[0].category, "GRAPHICS_FPS") != 0 ||
        strcmp(index.items[0].description, "Unlock FPS") != 0)
        return 21;
    if (strcmp(index.items[1].category, "COMMUNITY_CODE") != 0)
        return 22;

    if (write_index(path, invalid) != 0)
        return 9;
    if (ps3scale_community_index_load(path, &index) >= 0)
        return 10;
    if (index.count != 6)
        return 11;

    if (write_index(path, invalid_path) != 0)
        return 12;
    if (ps3scale_community_index_load(path, &index) >= 0)
        return 13;
    if (index.count != 6)
        return 14;

    ps3scale_community_index_free(&index);
    unlink(path);
    puts("PS3 SCALE community index tests: PASS");
    return 0;
}
