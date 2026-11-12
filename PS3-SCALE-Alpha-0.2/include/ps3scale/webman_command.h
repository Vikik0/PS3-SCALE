#ifndef PS3SCALE_WEBMAN_COMMAND_H
#define PS3SCALE_WEBMAN_COMMAND_H

#include <stddef.h>
#include "ps3scale/game.h"

/*
 * Build a validated webMAN /play.ps3 request path for a PS3 title.
 * Installed HDD titles use /play.ps3?<TITLE_ID>; folder/ISO titles use
 * /play.ps3/<URL-encoded-path>. Returns 0 on success, -1 on invalid input.
 */
int ps3scale_webman_build_start_path(const ps3scale_game_t *game,
                                     char *output, size_t output_size);

#endif
