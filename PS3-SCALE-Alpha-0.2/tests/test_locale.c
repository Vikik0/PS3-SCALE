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

    length = ps3scale_locale_to_glyphs("سلام", glyphs, sizeof(glyphs));
    assert(length == 4);
    assert((unsigned char)glyphs[0] >= 128);
    assert((unsigned char)glyphs[1] >= 128);
    assert((unsigned char)glyphs[2] >= 128);
    assert((unsigned char)glyphs[3] >= 128);

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
