#ifndef CB_GAME_LAUNCH_ARGS_H
#define CB_GAME_LAUNCH_ARGS_H

#include <stddef.h>
#include "online.h"

#define CB_GAME_LAUNCH_ARGV_CAPACITY 10
#define CB_GAME_NETPLAY_PORT "55435"

/* Builds RetroArch arguments for offline menu, host, direct client or relay client. */
int cb_game_build_argv(const char *content_path, int is_host,
                       const char *nickname,
                       const CBOnlineNetplayEndpoint *endpoint,
                       const char **argv, size_t capacity);

#endif
