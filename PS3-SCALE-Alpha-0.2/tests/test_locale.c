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

    /* Mixed RTL Arabic + LTR product names and version numbers stay intact. */
    length = ps3scale_locale_to_glyphs("إصدار PS3 4.92", glyphs,
                                       sizeof(glyphs));
    assert(length > 0);
    assert(strstr(glyphs, "PS3") != NULL);
    assert(strstr(glyphs, "4.92") != NULL);

    /* Arabic word spacing remains a real space, not an inter-letter gap. */
    length = ps3scale_locale_to_glyphs("مستوى الصوت", glyphs,
                                       sizeof(glyphs));
    assert(length == 11);
    assert(memchr(glyphs, ' ', (size_t)length) != NULL);

    /* Every required Arabic UI label must map to embedded Arabic glyphs. */
    {
        static const char *const required_arabic[] = {
            "الإعدادات",
            "مكتبة الألعاب",
            "البحث عن الألعاب",
            "مستوى الصوت",
            "تشغيل الموسيقى"
        };
        size_t sample;
        for (sample = 0; sample < sizeof(required_arabic) /
                                  sizeof(required_arabic[0]); sample++) {
            int glyph_index;
            length = ps3scale_locale_to_glyphs(required_arabic[sample],
                                               glyphs, sizeof(glyphs));
            assert(length > 0);
            for (glyph_index = 0; glyph_index < length; glyph_index++) {
                unsigned char glyph = (unsigned char)glyphs[glyph_index];
                assert(glyph == ' ' || ps3scale_locale_is_arabic_glyph(glyph));
            }
        }
    }

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

    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "ONLINE GRAPHICS DATABASE", translated, sizeof(translated)) != 1 ||
        strcmp(translated, "قاعدة بيانات تحسين الرسوم") != 0)
        return 8;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "PROFILES: 40  |  EXACT LIBRARY MATCHES: 2",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "40") == NULL || strstr(translated, "2") == NULL)
        return 9;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "WAIT FOR ONLINE DATABASE SYNC TO FINISH.",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "مزامنة قاعدة البيانات") == NULL)
        return 13;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "MOUNT THE EXACT GAME VERSION BEFORE ARMING THIS PROFILE.",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "إصدار اللعبة المطابق") == NULL)
        return 14;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "ONE-LAUNCH PROFILE ARMED. START THE MOUNTED GAME.",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "مرة واحدة") == NULL)
        return 10;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING.",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "قديم") == NULL)
        return 11;
    if (ps3scale_locale_translate(PS3SCALE_LANG_ARABIC,
            "ONE-LAUNCH PROFILE WILL CHECK THE EXACT ADDRESS AND ORIGINAL BYTES.",
            translated, sizeof(translated)) != 1 ||
        strstr(translated, "البايتات الأصلية") == NULL)
        return 12;

    puts("PS3 SCALE locale tests: PASS");
    return 0;
}
