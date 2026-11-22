#include "game_launch_args.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *argv[CB_GAME_LAUNCH_ARGV_CAPACITY];
    const char *content = "/dev_hdd0/PSXISO/Crash Bash.cue";
    CBOnlineNetplayEndpoint endpoint;

    memset(&endpoint, 0, sizeof(endpoint));
    assert(cb_game_build_argv(content, 1, "CB-ABCDEF012345-HOST", NULL, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 6);
    assert(strcmp(argv[0], content) == 0);
    assert(strcmp(argv[1], "--host") == 0);
    assert(strcmp(argv[2], "--nick") == 0);
    assert(strcmp(argv[3], "CB-ABCDEF012345-HOST") == 0);
    assert(strcmp(argv[4], "--port") == 0);
    assert(strcmp(argv[5], "55435") == 0);
    assert(argv[6] == NULL);

    assert(cb_game_build_argv(content, 0, "LocalPlayer", NULL, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 6);
    assert(strcmp(argv[1], "--menu") == 0);
    assert(strcmp(argv[3], "LocalPlayer") == 0);
    assert(argv[6] == NULL);

    snprintf(endpoint.address, sizeof(endpoint.address), "madrid.relay.retroarch.com");
    snprintf(endpoint.port, sizeof(endpoint.port), "55435");
    snprintf(endpoint.mitm_session, sizeof(endpoint.mitm_session), "AbCdEf0123456789");
    snprintf(endpoint.mode, sizeof(endpoint.mode), "relay");
    assert(cb_game_build_argv(content, 0, "CB-ABCDEF012345-P2", &endpoint, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 9);
    assert(strcmp(argv[1], "--connect") == 0);
    assert(strcmp(argv[2], "madrid.relay.retroarch.com") == 0);
    assert(strcmp(argv[3], "--port") == 0);
    assert(strcmp(argv[4], "55435") == 0);
    assert(strcmp(argv[5], "--mitm-session") == 0);
    assert(strcmp(argv[6], "AbCdEf0123456789") == 0);
    assert(strcmp(argv[7], "--nick") == 0);
    assert(strcmp(argv[8], "CB-ABCDEF012345-P2") == 0);
    assert(argv[9] == NULL);

    endpoint.mitm_session[0] = '\0';
    snprintf(endpoint.address, sizeof(endpoint.address), "203.0.113.10");
    assert(cb_game_build_argv(content, 0, "DirectPlayer", &endpoint, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == 7);
    assert(strcmp(argv[1], "--connect") == 0);
    assert(strcmp(argv[2], "203.0.113.10") == 0);
    assert(strcmp(argv[5], "--nick") == 0);
    assert(strcmp(argv[6], "DirectPlayer") == 0);
    assert(argv[7] == NULL);

    assert(cb_game_build_argv(NULL, 1, "HOST", NULL, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == -1);
    assert(cb_game_build_argv(content, 1, "", NULL, argv, CB_GAME_LAUNCH_ARGV_CAPACITY) == -1);
    assert(cb_game_build_argv(content, 0, "Client", &endpoint, argv, 6) == -1);

    puts("Game launch argument tests passed.");
    return 0;
}
