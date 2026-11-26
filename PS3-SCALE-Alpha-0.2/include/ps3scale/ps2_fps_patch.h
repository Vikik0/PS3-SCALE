#ifndef PS3SCALE_PS2_FPS_PATCH_H
#define PS3SCALE_PS2_FPS_PATCH_H

#include <stdint.h>

typedef struct {
    const char *serial;
    uint32_t pcsx2_elf_hash;
    const char *title;
    const char *source_url;
    uint32_t ee_address;
    uint32_t replacement_word;
} ps3scale_ps2_fps_patch_t;

/* Returns a patch only for an exact serial + PCSX2 ELF hash match. */
const ps3scale_ps2_fps_patch_t *ps3scale_ps2_fps_patch_find(
    const char *serial, uint32_t pcsx2_elf_hash);

#endif
