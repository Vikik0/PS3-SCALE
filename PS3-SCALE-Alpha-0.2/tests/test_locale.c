#include "ps3scale/locale.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char translated[256];
    char glyphs[256];
    int length;

    assert(ps3scale_locale_translate(PS3SCALE_LANG_ARABIC, "SETTINGS",
                                     translated, sizeof(translated)) == 1);
    assert(strcmp(translated, "الإعدادات") == 0);

    assert(ps3scale_locale_translate(PS3SCALE_LANG_SPANISH, "SETTINGS",
                                     translated, sizeof(translated)) == 1);
    assert(strcmp(translated, "Configuración") == 0);

    assert(ps3scale_locale_glyph_font_index((unsigned char)'A') == 0);
    assert(ps3scale_locale_glyph_font_index((unsigned char)'9') == 0);
    assert(ps3scale_locale_glyph_font_index(128u) == 1);
    assert(ps3scale_locale_glyph_font_index(254u) == 1);

    length = ps3scale_locale_to_glyphs("سلام", glyphs, sizeof(glyphs));
    assert(length == 4);
    assert((unsigned char)glyphs[0] >= 128);
    assert((unsigned char)glyphs[1] >= 128);
    assert((unsigned char)glyphs[2] >= 128);
    assert((unsigned char)glyphs[3] >= 128);
    /* Arabic shaping is visual RTL order with the correct contextual forms. */
    assert((unsigned char)glyphs[0] == 225u); /* meem isolated */
    assert((unsigned char)glyphs[1] == 142u); /* alif final */
    assert((unsigned char)glyphs[2] == 224u); /* lam medial */
    assert((unsigned char)glyphs[3] == 179u); /* seen initial */
    assert(ps3scale_locale_is_arabic_glyph(225u));
    assert(!ps3scale_locale_is_arabic_glyph(246u));
    assert(ps3scale_locale_glyphs_joined(142u, 224u));
    assert(ps3scale_locale_glyphs_joined(224u, 179u));
    assert(!ps3scale_locale_glyphs_joined(225u, 142u));

    length = ps3scale_locale_to_glyphs("الإعدادات", glyphs,
                                       sizeof(glyphs));
    assert(length == 9);
    for (int i = 0; i < length; i++)
        assert((unsigned char)glyphs[i] >= 128u);

    /* English labels and digits must remain in their original left-to-right order. */
    length = ps3scale_locale_to_glyphs("PS3 SCALE 4.92", glyphs,
                                       sizeof(glyphs));
    assert(length == (int)strlen("PS3 SCALE 4.92"));
    assert(strcmp(glyphs, "PS3 SCALE 4.92") == 0);

    /* Arabic word spacing remains a real space, not an inter-letter gap. */
    length = ps3scale_locale_to_glyphs("مستوى الصوت", glyphs,
                                       sizeof(glyphs));
    assert(length == 11);
    assert(memchr(glyphs, ' ', (size_t)length) != NULL);

    length = ps3scale_locale_to_glyphs("Configuración", glyphs,
                                       sizeof(glyphs));
    assert(length == (int)strlen("Configuración") - 1);
    assert(memchr(glyphs, 249, (size_t)length) != NULL);

    assert(strcmp(ps3scale_locale_keyboard_key(PS3SCALE_LANG_ARABIC, 10),
                  "ا") == 0);
    assert(strcmp(ps3scale_locale_keyboard_key(PS3SCALE_LANG_SPANISH, 37),
                  "ñ") == 0);
    assert(strcmp(ps3scale_locale_keyboard_key(PS3SCALE_LANG_ENGLISH, 10),
                  "Q") == 0);
    assert(strcmp(ps3scale_locale_language_name(PS3SCALE_LANG_ARABIC),
                  "العربية") == 0);

    puts("PS3 SCALE locale tests: PASS");
    return 0;
}
