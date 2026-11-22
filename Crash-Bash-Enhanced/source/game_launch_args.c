#include "game_launch_args.h"

int cb_game_build_argv(const char *content_path, int is_host,
                       const char *nickname, const char **argv, size_t capacity)
{
    if (!content_path || !content_path[0] ||
        !nickname || !nickname[0] ||
        !argv || capacity < CB_GAME_LAUNCH_ARGV_CAPACITY) {
        return -1;
    }

    argv[0] = content_path;
    argv[1] = is_host ? "--host" : "--menu";
    argv[2] = "--nick";
    argv[3] = nickname;
    argv[4] = "--port";
    argv[5] = CB_GAME_NETPLAY_PORT;
    argv[6] = NULL;
    return 6;
}
