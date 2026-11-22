#include "game_launch_args.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *argv[CB_GAME_LAUNCH_ARGV_CAPACITY];
    const char *content = "/dev_hdd0/PSXISO/Crash Bash.cue";

    assert(cb_game_build_argv(content, 1, "CB-ABC234-HOST",
                              argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 6);
    assert(strcmp(argv[0], content) == 0);
    assert(strcmp(argv[1], "--host") == 0);
    assert(strcmp(argv[2], "--nick") == 0);
    assert(strcmp(argv[3], "CB-ABC234-HOST") == 0);
    assert(strcmp(argv[4], "--port") == 0);
    assert(strcmp(argv[5], "55435") == 0);
    assert(argv[6] == NULL);

    assert(cb_game_build_argv(content, 0, "CB-ABC234-P2",
                              argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 6);
    assert(strcmp(argv[1], "--menu") == 0);
    assert(strcmp(argv[3], "CB-ABC234-P2") == 0);
    assert(argv[6] == NULL);

    assert(cb_game_build_argv(NULL, 1, "HOST", argv,
                              CB_GAME_LAUNCH_ARGV_CAPACITY) == -1);
    assert(cb_game_build_argv(content, 1, "", argv,
                              CB_GAME_LAUNCH_ARGV_CAPACITY) == -1);
    assert(cb_game_build_argv(content, 1, "HOST", argv, 6) == -1);

    puts("Game launch argument tests passed.");
    return 0;
}
