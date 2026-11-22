#ifndef CB_GAME_LAUNCHER_H
#define CB_GAME_LAUNCHER_H

#include <stddef.h>
#include "online.h"

/* Starts the installed RetroArch app with the selected content and network role.
   A NULL endpoint opens the local menu for offline use or starts the host. */
int cb_launch_game(const char *content_path, int is_host, const char *nickname,
                   const CBOnlineNetplayEndpoint *endpoint,
                   char *error, size_t error_size);

#endif
