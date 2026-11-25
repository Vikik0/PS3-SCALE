#ifndef PS3SCALE_SUPPORT_PAYLOAD_H
#define PS3SCALE_SUPPORT_PAYLOAD_H

#include <stddef.h>

#define PS3SCALE_SUPPORT_TITLE_MAX 100u
#define PS3SCALE_SUPPORT_DETAILS_MAX 2000u
#define PS3SCALE_SUPPORT_REQUEST_ID_LENGTH 36u

/*
 * Build a bounded JSON report payload. Strings are escaped as JSON, the
 * request ID must be a UUID, and user text is capped to the server limits.
 */
int ps3scale_support_build_json(
    char *destination,
    size_t capacity,
    const char *request_id,
    const char *kind,
    const char *title,
    const char *details,
    const char *model,
    const char *firmware,
    const char *platform,
    const char *app_version);

#endif
