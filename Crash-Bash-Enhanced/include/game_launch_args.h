#ifndef CB_GAME_LAUNCH_ARGS_H
#define CB_GAME_LAUNCH_ARGS_H

#include <stddef.h>

#define CB_GAME_LAUNCH_ARGV_CAPACITY 7
#define CB_GAME_NETPLAY_PORT "55435"

/* argv is the PS3 process argument vector: its first entry is the CUE path. */
int cb_game_build_argv(const char *content_path, int is_host,
                       const char *nickname, const char **argv, size_t capacity);

#endif
