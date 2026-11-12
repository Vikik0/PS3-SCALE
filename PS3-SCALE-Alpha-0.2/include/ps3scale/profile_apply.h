#ifndef PS3SCALE_PROFILE_APPLY_H
#define PS3SCALE_PROFILE_APPLY_H

#include "ps3scale/catalog.h"

/*
 * Returns 1 only when the installed one-launch script's complete contents
 * match the deterministic PS3 SCALE script for the exact Title ID, APP_VER
 * and catalog patch data. Returns -2 for stale, legacy or modified PS3 SCALE
 * scripts, -1 for a foreign script or invalid identity, and 0 when no script
 * exists for this Title ID.
 */
int ps3scale_profile_script_state(const char *title_id, const char *app_ver,
                                  const char *patches);

/*
 * Installs a one-launch webMAN script after explicit user confirmation.
 * Only one-write B_COMMUNITY profiles are supported in this build.
 * The script checks one exact candidate address and the expected original
 * bytes, deletes itself after the launch attempt, and never claims verified
 * success. A modified script is treated as stale before the game can launch.
 * Returns 0 on install, 1 if already installed, negative on refusal/failure.
 */
int ps3scale_profile_script_install(const ps3scale_profile_t *profile);

/* Removes only a PS3 SCALE-owned V1/V2 script, never a foreign user script. */
int ps3scale_profile_script_remove(const char *title_id);

#endif
