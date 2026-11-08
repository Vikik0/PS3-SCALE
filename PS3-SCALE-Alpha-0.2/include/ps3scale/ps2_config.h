#ifndef PS3SCALE_PS2_CONFIG_H
#define PS3SCALE_PS2_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define PS3SCALE_PS2_CONFIG_MAX_BYTES 16384u

#define PS3SCALE_PS2_CFG_DISABLE_SMOOTHING 0x01u
#define PS3SCALE_PS2_CFG_ENABLE_L2H        0x02u

/*
 * Build a conservative ps2_netemu CONFIG template in memory.
 * This function does not write files or modify an ISO.
 *
 * DISABLE_SMOOTHING: netemu command 0x44; disables the emulator smoothing
 * filter. This may look sharper but can increase visible aliasing.
 *
 * ENABLE_L2H: netemu command 0x46; enables the documented L2H improvement.
 * Performance gains are title-dependent and are not guaranteed.
 *
 * Returns 0 on success, negative on invalid flags or insufficient capacity.
 */
int ps3scale_ps2_config_build(uint8_t *out, size_t capacity,
                              unsigned flags, size_t *out_size);

/* Validates only the restricted CONFIG format emitted by the builder above. */
int ps3scale_ps2_config_validate_generated(const uint8_t *data, size_t size);

#endif
