#ifndef CB_GAME_LAUNCHER_H
#define CB_GAME_LAUNCHER_H

#include <stddef.h>

/* Launches the separate RetroArch + PCSX-ReARMed Crash Bash package.
   Returns -1 with an explanatory message only if the target cannot be launched. */
int cb_launch_game(char *error, size_t error_size);

#endif
