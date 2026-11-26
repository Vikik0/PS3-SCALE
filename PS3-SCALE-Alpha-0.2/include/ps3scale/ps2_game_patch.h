#ifndef PS3SCALE_PS2_GAME_PATCH_H
#define PS3SCALE_PS2_GAME_PATCH_H
#include <stddef.h>
#include <stdint.h>
typedef enum {
 PS3SCALE_PS2_PATCH_FPS=1,
 PS3SCALE_PS2_PATCH_WIDESCREEN=2,
 PS3SCALE_PS2_PATCH_480P=3
} ps3scale_ps2_patch_kind_t;
typedef struct { uint32_t ee_address; uint32_t replacement_word; } ps3scale_ps2_patch_word_t;
typedef struct {
 const char *serial; uint32_t pcsx2_elf_hash; const char *title;
 const char *source_url; ps3scale_ps2_patch_kind_t kind;
 const ps3scale_ps2_patch_word_t *words; size_t word_count;
} ps3scale_ps2_game_patch_t;
const ps3scale_ps2_game_patch_t *ps3scale_ps2_game_patch_find(
 const char *serial,uint32_t pcsx2_elf_hash,ps3scale_ps2_patch_kind_t kind);
#endif
