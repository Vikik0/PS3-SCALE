#ifndef PS3SCALE_PS2_CONFIG_PATH_H
#define PS3SCALE_PS2_CONFIG_PATH_H

#include <stddef.h>

#define PS3SCALE_PS2_HDD_CACHE_DIR "/dev_hdd0/PS2ISO"

/*
 * Resolve the CONFIG sidecar path actually consumed by webMAN MOD.
 *
 * Internal HDD images use <image>.CONFIG beside the ISO.
 * External USB images are copied by webMAN to /dev_hdd0/PS2ISO/<basename>
 * before mounting, so their CONFIG must be written beside that cached copy.
 *
 * Returns 0 on success, -1 for an unsupported/unsafe path or short output.
 * uses_hdd_cache is optional and is set to 1 for external USB images.
 */
int ps3scale_ps2_iso_config_path(const char *iso_path,
                                 char *output, size_t output_size,
                                 int *uses_hdd_cache);

#endif
