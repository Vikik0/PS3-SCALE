#ifndef CB_GAME_LAUNCHER_H
#define CB_GAME_LAUNCHER_H

#include <stddef.h>

/* Starts the installed RetroArch app with the selected CUE and room role.
   Host starts Netplay; clients open the in-game menu to select the host lobby. */
int cb_launch_game(const char *content_path, int is_host, const char *nickname,
                   char *error, size_t error_size);

#endif
