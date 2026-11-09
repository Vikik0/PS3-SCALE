#include "ps3scale/webman_command.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_path(ps3scale_game_t *game, const char *expected)
{
    char output[1537];
    assert(ps3scale_webman_build_start_path(game, output, sizeof(output)) == 0);
    assert(strcmp(output, expected) == 0);
}

int main(void)
{
    ps3scale_game_t game;
    char output[1537];

    memset(&game, 0, sizeof(game));
    game.kind = PS3SCALE_GAME_PS3_HDD;
    strcpy(game.title_id, "RXKIR1131");
    strcpy(game.path, "/dev_hdd0/game/RXKIR1131");
    expect_path(&game, "/play.ps3?RXKIR1131");

    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.path, "/dev_hdd0/GAMES/Kirby & Friends");
    expect_path(&game, "/play.ps3/dev_hdd0/GAMES/Kirby%20%26%20Friends");

    game.kind = PS3SCALE_GAME_PS3_ISO;
    strcpy(game.path, "/dev_usb000/PS3ISO/Gran Turismo 6.iso");
    expect_path(&game,
        "/play.ps3/dev_usb000/PS3ISO/Gran%20Turismo%206.iso");

    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.path, "/dev_bdvd");
    expect_path(&game, "/play.ps3");

    game.kind = PS3SCALE_GAME_PS3_HDD;
    strcpy(game.title_id, "rxkir1131");
    assert(ps3scale_webman_build_start_path(&game, output,
                                             sizeof(output)) != 0);

    strcpy(game.title_id, "RXKIR1131");
    game.kind = PS3SCALE_GAME_PS3_FOLDER;
    strcpy(game.path, "/dev_hdd0/GAMES/../dev_flash");
    assert(ps3scale_webman_build_start_path(&game, output,
                                             sizeof(output)) != 0);

    strcpy(game.path, "/dev_hdd0/GAMES//BadPath");
    assert(ps3scale_webman_build_start_path(&game, output,
                                             sizeof(output)) != 0);

    game.kind = PS3SCALE_GAME_PS2_ISO;
    strcpy(game.path, "/dev_hdd0/PS2ISO/Crash Twinsanity [SLUS_209.09].iso");
    expect_path(&game,
        "/play.ps3/dev_hdd0/PS2ISO/Crash%20Twinsanity%20%5BSLUS_209.09%5D.iso");

    strcpy(game.path, "/dev_usb001/PS2ISO/Crash.bin");
    expect_path(&game, "/play.ps3/dev_usb001/PS2ISO/Crash.bin");

    strcpy(game.path, "/dev_hdd0/GAMES/Crash.iso");
    assert(ps3scale_webman_build_start_path(&game, output,
                                             sizeof(output)) != 0);
    strcpy(game.path, "/dev_hdd0/PS2ISO/Crash.bin.enc");
    assert(ps3scale_webman_build_start_path(&game, output,
                                             sizeof(output)) != 0);

    game.kind = PS3SCALE_GAME_PS3_HDD;
    assert(ps3scale_webman_build_start_path(&game, output, 4) != 0);
    assert(output[0] == '\0');

    puts("PS3 SCALE webMAN launch route tests: PASS");
    return 0;
}
