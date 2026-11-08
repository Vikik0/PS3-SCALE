#ifndef PS3SCALE_PROFILE_APPLY_H
#define PS3SCALE_PROFILE_APPLY_H

#include "ps3scale/catalog.h"

/*
 * A profile script is installed only after an explicit controller action.
 * It is scoped to one Title ID and writes only when the expected bytes match.
 */
int ps3scale_profile_script_state(const char *title_id, const char *app_ver);

/* Returns 0 on install, 1 if already installed, negative on refusal/failure. */
int ps3scale_profile_script_install(const ps3scale_profile_t *profile);

/* Removes only a script carrying the PS3 SCALE ownership header. */
int ps3scale_profile_script_remove(const char *title_id);

#endif
