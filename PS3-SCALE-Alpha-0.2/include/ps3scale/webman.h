#ifndef PS3SCALE_WEBMAN_H
#define PS3SCALE_WEBMAN_H

/* Returns 0 only when the local webMAN MOD HTTP service responds. */
int ps3scale_webman_available(void);

/* Returns 0 only when the local PS3MAPI HTTP endpoint responds. */
int ps3scale_webman_ps3mapi_available(void);

/*
 * Plays a webMAN sound popup and starts the already-mounted PS3 disc title
 * through /play.ps3. The caller must verify the mounted Title ID first.
 */
int ps3scale_webman_start_mounted_game(void);

#endif
