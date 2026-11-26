#include "ps3scale/ps2_fps_patch.h"

#include <string.h>

/*
 * Experimental 60 FPS entries copied from the 60 FPS sections of the
 * PCSX2 patch repository at the pinned commit below. The original opcode is
 * read from the exact matched boot ELF at runtime; it is never guessed.
 * These are candidates for PS3 netemu testing, not hardware-verified patches.
 */
#define PCSX2_PATCH_COMMIT "1fd98102b8e2fffe3e0c29aab55339d48af0ffe5"
#define PCSX2_PATCH_URL(file) \
    "https://github.com/PCSX2/pcsx2_patches/blob/" PCSX2_PATCH_COMMIT "/patches/" file

static const ps3scale_ps2_fps_patch_t g_ps2_fps_patches[] = {
    {
        "SLUS_212.60", 0xB0AE2D8Au,
        "Ed, Edd n Eddy: The Mis-Edventures (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21260_B0AE2D8A.pnach"),
        0x00161860u, 0x2C620000u
    },
    {
        "SLUS_203.49", 0x10B1F370u,
        "Scooby-Doo! Night of 100 Frights (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-20349_10B1F370.pnach"),
        0x00131DB0u, 0x2C420001u
    },
    {
        "SLUS_211.83", 0xE1F17139u,
        "Teen Titans (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21183_E1F17139.pnach"),
        0x0015FF94u, 0x2C620000u
    },
    {
        "SLUS_214.37", 0x86B490D1u,
        "Disney's Kim Possible - What's the Switch (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21437_86B490D1.pnach"),
        0x0015C844u, 0x2C620000u
    },
    {
        "SLUS_214.55", 0xBAFEFC57u,
        "Happy Feet (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21455_BAFEFC57.pnach"),
        0x0015CDD4u, 0x2C620000u
    },
    {
        "SLUS_214.15", 0xE10A865Bu,
        "The Ant Bully (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21415_E10A865B.pnach"),
        0x0015CCD4u, 0x2C620000u
    },
    {
        "SLUS_218.85", 0xB15F0BD6u,
        "Indiana Jones and the Staff of Kings (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-21885_B15F0BD6.pnach"),
        0x001505B4u, 0x2C620000u
    },
    {
        "SLUS_206.96", 0xFCC09959u,
        "Jimmy Neutron: Jet Fusion (NTSC-U)",
        PCSX2_PATCH_URL("SLUS-20696_FCC09959.pnach"),
        0x0022CA0Cu, 0x28420001u
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
