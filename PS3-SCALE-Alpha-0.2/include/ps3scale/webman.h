#ifndef PS3SCALE_WEBMAN_H
#define PS3SCALE_WEBMAN_H

#include "ps3scale/game.h"

/* Returns 0 only when the local webMAN MOD HTTP service responds. */
int ps3scale_webman_available(void);

/* Returns 0 only when the local PS3MAPI HTTP endpoint responds. */
int ps3scale_webman_ps3mapi_available(void);

/*
 * Ask webMAN MOD to launch the selected PS3 title.
 * - Installed HDD/NPDRM titles use /play.ps3?<TITLE_ID>.
 * - PS3 folder/ISO titles use /play.ps3<URL-encoded-path>.
 * This function performs bounded network I/O and must be called by a worker.
 */
int ps3scale_webman_start_game(const ps3scale_game_t *game);

#endif
