#ifndef PS3SCALE_ONLINE_H
#define PS3SCALE_ONLINE_H

#include <stddef.h>

/* Downloads a fixed HTTPS resource to a local file using verified TLS. */
int ps3scale_online_download(const char *url, const char *destination);

/* Same verified HTTPS path with an explicit, bounded maximum file size. */
int ps3scale_online_download_limited(
    const char *url,
    const char *destination,
    size_t max_bytes);

/* Sends a small JSON report over HTTPS with certificate verification. */
int ps3scale_online_post_json(
    const char *url,
    const char *json,
    size_t json_size,
    long *response_code);

#endif
