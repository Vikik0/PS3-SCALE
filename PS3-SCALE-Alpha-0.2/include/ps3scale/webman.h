#ifndef PS3SCALE_WEBMAN_H
#define PS3SCALE_WEBMAN_H

#include "ps3scale/game.h"

enum {
    PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST = -10,
    PS3SCALE_WEBMAN_ERROR_NETWORK_INIT = -11,
    PS3SCALE_WEBMAN_ERROR_SOCKET = -12,
    PS3SCALE_WEBMAN_ERROR_CONNECT = -13,
    PS3SCALE_WEBMAN_ERROR_SEND = -14,
    PS3SCALE_WEBMAN_ERROR_RESPONSE = -15,
    PS3SCALE_WEBMAN_ERROR_HTTP_STATUS = -16
};

/* Returns 0 only when the local webMAN MOD HTTP service responds. */
int ps3scale_webman_available(void);

/* Returns 0 only when the local PS3MAPI HTTP endpoint responds. */
int ps3scale_webman_ps3mapi_available(void);

/*
 * Ask webMAN MOD to launch the selected PS3 title.
 * - Installed HDD/NPDRM titles use /play.ps3?<TITLE_ID>.
 * - PS3 folder/ISO titles use /play.ps3<URL-encoded-path>.
 * Returns a PS3SCALE_WEBMAN_ERROR_* code on failure.
 * This function performs bounded network I/O and must be called by a worker.
 */
int ps3scale_webman_start_game(const ps3scale_game_t *game);

/* Ask webMAN MOD to launch an installed application by its validated Title ID. */
int ps3scale_webman_start_app(const char *title_id);

#endif
