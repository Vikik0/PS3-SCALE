#include "game_launch_args.h"

#include <stdio.h>
#include <string.h>

int cb_game_build_argv(const char *content_path, int is_host,
                       const char *nickname,
                       const CBOnlineNetplayEndpoint *endpoint,
                       const char **argv, size_t capacity)
{
    size_t used = 0;

    if (!content_path || !content_path[0] ||
        !nickname || !nickname[0] ||
        !argv || capacity < CB_GAME_LAUNCH_ARGV_CAPACITY) {
        return -1;
    }

    argv[used++] = content_path;

    if (is_host) {
        argv[used++] = "--host";
        argv[used++] = "--nick";
        argv[used++] = nickname;
        argv[used++] = "--port";
        argv[used++] = CB_GAME_NETPLAY_PORT;
    } else if (endpoint && endpoint->address[0] && endpoint->port[0]) {
        argv[used++] = "--connect";
        argv[used++] = endpoint->address;
        argv[used++] = "--port";
        argv[used++] = endpoint->port;
        if (endpoint->mitm_session[0]) {
            argv[used++] = "--mitm-session";
            argv[used++] = endpoint->mitm_session;
        }
        argv[used++] = "--nick";
        argv[used++] = nickname;
    }
    /*
     * Local single-player launch: argv[0] is the CUE path. Do not pass
     * --menu here; RetroArch's --menu option intentionally starts without
     * loading content, which would leave Crash Bash unopened.
     */

    if (used >= capacity) return -1;
    argv[used] = NULL;
    return (int)used;
}
