#include "ps3scale/ps2_fps_patch.h"

#include <string.h>

/*
 * Proof-of-concept conversion from the pinned PCSX2 60 FPS pnach.
 * The original opcode is read from the exact matched boot ELF at runtime;
 * it is deliberately not guessed or copied from a comment.
 */
static const ps3scale_ps2_fps_patch_t g_ps2_fps_patches[] = {
    {
        "SLUS_212.60",
        0xB0AE2D8Au,
        "Ed, Edd n Eddy: The Mis-Edventures (NTSC-U)",
        "https://github.com/PCSX2/pcsx2_patches/blob/1fd98102b8e2fffe3e0c29aab55339d48af0ffe5/patches/SLUS-21260_B0AE2D8A.pnach",
        0x00161860u,
        0x2C620000u
    }
};

const ps3scale_ps2_fps_patch_t *ps3scale_ps2_fps_patch_find(
    const char *serial, uint32_t pcsx2_elf_hash)
{
    size_t i;

    if (!serial || !serial[0])
        return NULL;

    for (i = 0; i < sizeof(g_ps2_fps_patches) /
                    sizeof(g_ps2_fps_patches[0]); ++i) {
        if (strcmp(serial, g_ps2_fps_patches[i].serial) == 0 &&
            pcsx2_elf_hash == g_ps2_fps_patches[i].pcsx2_elf_hash)
            return &g_ps2_fps_patches[i];
    }

    return NULL;
}
