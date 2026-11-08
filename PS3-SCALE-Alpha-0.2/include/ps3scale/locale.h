#ifndef PS3SCALE_LOCALE_H
#define PS3SCALE_LOCALE_H

#include <stddef.h>

enum {
    PS3SCALE_LANG_ENGLISH = 0,
    PS3SCALE_LANG_ARABIC = 1,
    PS3SCALE_LANG_SPANISH = 2
};

/* Returns 1 when a translation is used, otherwise copies the source text. */
int ps3scale_locale_translate(int language, const char *source,
                              char *output, size_t output_size);

/* Converts UTF-8 UI text to the PS3 SCALE extended 8-bit bitmap-font codes. */
int ps3scale_locale_to_glyphs(const char *source, char *output,
                              size_t output_size);

/* Font 0 contains ASCII; font 1 contains the extended glyph atlas. */
int ps3scale_locale_glyph_font_index(unsigned char glyph);

const char *ps3scale_locale_language_name(int language);
const char *ps3scale_locale_keyboard_key(int language, int index);

#endif
