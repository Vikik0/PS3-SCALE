#include "ps3scale/locale.h"
#include "../ps3/arabic_glyph_map.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct locale_entry {
    const char *english;
    const char *arabic;
    const char *spanish;
} locale_entry_t;

static const locale_entry_t locale_entries[] = {
    {"GAME LIBRARY  /  GRAPHICS PROFILES  /  SAFE TOOLS", "مكتبة الألعاب / ملفات الرسوم / أدوات آمنة", "Biblioteca / perfiles gráficos / herramientas seguras"},
    {"MAIN MENU", "القائمة الرئيسية", "Menú principal"},
    {"SCAN GAMES", "فحص الألعاب", "Buscar juegos"},
    {"SETTINGS", "الإعدادات", "Configuración"},
    {"COMPANION TOOLS", "الأدوات المساعدة", "Herramientas auxiliares"},
    {"ABOUT PS3 SCALE", "حول PS3 SCALE", "Acerca de PS3 SCALE"},
    {"LIBRARY OVERVIEW", "ملخص المكتبة", "Resumen de biblioteca"},
    {"SYSTEM STATUS", "حالة النظام", "Estado del sistema"},
    {"MUSIC: ON", "الموسيقى: تشغيل", "Música: activada"},
    {"MUSIC: OFF", "الموسيقى: إيقاف", "Música: desactivada"},
    {"HOME ANIMATION: ON", "حركة الرئيسية: تشغيل", "Animación inicial: activada"},
    {"HOME ANIMATION: OFF", "حركة الرئيسية: إيقاف", "Animación inicial: desactivada"},
    {"GAME FILES: READ-ONLY", "ملفات الألعاب: قراءة فقط", "Archivos de juego: solo lectura"},
    {"UP/DOWN: SELECT   CROSS: OPEN   CIRCLE: EXIT", "أعلى/أسفل: اختيار   X: فتح   O: خروج", "Arriba/abajo: elegir   X: abrir   O: salir"},
    {"LOCAL LIBRARY SCAN  |  NO INTERNET REQUIRED", "فحص المكتبة محلياً | لا يلزم الإنترنت", "Escaneo local | no se requiere Internet"},
    {"PLATFORM:", "المنصة:", "Plataforma:"},
    {"L1  PS3", "L1  PS3", "L1  PS3"},
    {"R1  PS2", "R1  PS2", "R1  PS2"},
    {"SELECT/START: SEARCH", "SELECT/START: بحث", "SELECT/START: Buscar"},
    {"SEARCH: ALL GAMES", "بحث: كل الألعاب", "Buscar: todos los juegos"},
    {"PS3 GAME LIST", "قائمة ألعاب PS3", "Juegos de PS3"},
    {"PS2 GAME LIST", "قائمة ألعاب PS2", "Juegos de PS2"},
    {"SELECTED GAME", "اللعبة المحددة", "Juego seleccionado"},
    {"CROSS: OPEN DETAILS", "X: عرض التفاصيل", "X: ver detalles"},
    {"NO MATCHING GAMES FOUND.", "لم يتم العثور على ألعاب مطابقة.", "No se encontraron juegos."},
    {"PRESS SELECT OR START TO SEARCH.", "اضغط SELECT أو START للبحث.", "Pulsa SELECT o START para buscar."},
    {"WARNING: LIBRARY LIMIT REACHED (256 TITLES)", "تنبيه: تم بلوغ حد المكتبة (256 لعبة)", "Aviso: límite de biblioteca alcanzado (256 juegos)"},
    {"UP/DOWN MOVE  |  L1/R1 PLATFORM  |  CROSS DETAILS  |  SELECT/START SEARCH", "أعلى/أسفل للتنقل | L1/R1 للمنصة | X للتفاصيل | SELECT/START للبحث", "Arriba/abajo mover | L1/R1 plataforma | X detalles | SELECT/START buscar"},
    {"SQUARE RESCAN  |  TRIANGLE PS2 PREVIEW  |  CIRCLE BACK", "مربع: إعادة الفحص | مثلث: معاينة PS2 | دائرة: رجوع", "Cuadrado: reescanear | Triángulo: vista PS2 | Círculo: volver"},
    {"GAME DETAILS", "تفاصيل اللعبة", "Detalles del juego"},
    {"NO GAME SELECTED.", "لم يتم تحديد لعبة.", "No hay juego seleccionado."},
    {"GRAPHICS PROFILE", "ملف تحسين الرسوم", "Perfil gráfico"},
    {"NO VERIFIED PATCH MATCHED YET", "لا يوجد تعديل موثّق مطابق حالياً", "No hay un parche verificado compatible"},
    {"ORIGINAL GAME FILES HAVE NOT BEEN CHANGED.", "لم يتم تغيير ملفات اللعبة الأصلية.", "Los archivos originales no se han modificado."},
    {"PROFILE DOWNLOAD / BACKUP / ROLLBACK ARE NOT ACTIVE YET.", "تنزيل الملفات والنسخ الاحتياطي والاستعادة غير مفعّلة بعد.", "La descarga, copia y restauración aún no están activas."},
    {"PS2 CONFIG PREVIEW - NOT APPLIED", "معاينة إعداد PS2 - لم يتم تطبيقه", "Vista previa de CONFIG PS2 - no aplicado"},
    {"TRIANGLE: PREVIEW PS2 CONFIG", "مثلث: معاينة إعداد PS2", "Triángulo: vista previa CONFIG PS2"},
    {"TRIANGLE: CONFIG PREVIEW  |  CIRCLE: BACK TO GAME LIST", "مثلث: معاينة الإعداد | دائرة: العودة للقائمة", "Triángulo: vista previa | Círculo: volver a juegos"},
    {"BACKGROUND MUSIC", "موسيقى الخلفية", "Música de fondo"},
    {"MUSIC VOLUME", "مستوى الصوت", "Volumen de música"},
    {"HOME ANIMATION", "حركة الشاشة الرئيسية", "Animación de inicio"},
    {"CHECK FOR USB UPDATE", "البحث عن تحديث على USB", "Buscar actualización en USB"},
    {"RESCAN GAME LIBRARY", "إعادة فحص مكتبة الألعاب", "Volver a escanear juegos"},
    {"RESTORE DEFAULT SETTINGS", "استعادة الإعدادات الافتراضية", "Restaurar ajustes predeterminados"},
    {"COPY MUSIC TO INTERNAL HDD", "نسخ الموسيقى إلى القرص الداخلي", "Copiar música al disco interno"},
    {"LANGUAGE", "اللغة", "Idioma"},
    {"PERSONALIZE THE APP AND RUN SAFE MAINTENANCE TASKS.", "خصص البرنامج ونفّذ مهام الصيانة الآمنة.", "Personaliza la aplicación y ejecuta tareas seguras."},
    {"ON", "تشغيل", "Activado"},
    {"OFF", "إيقاف", "Desactivado"},
    {"CURRENT LIBRARY", "المكتبة الحالية", "Biblioteca actual"},
    {"DEFAULTS", "افتراضي", "Predeterminados"},
    {"IMPORT FROM USB", "استيراد من USB", "Importar desde USB"},
    {"SCAN USB000 - USB007", "فحص USB000 إلى USB007", "Escanear USB000 - USB007"},
    {"AUDIO ENGINE: MUSIC FILE NOT FOUND OR INIT FAILED", "الصوت: لم يتم العثور على الملف أو فشل التشغيل", "Audio: archivo no encontrado o error de inicio"},
    {"SETTINGS COULD NOT BE SAVED TO INTERNAL HDD.", "تعذر حفظ الإعدادات على القرص الداخلي.", "No se pudieron guardar los ajustes."},
    {"SETTINGS SAVE FAILED.", "فشل حفظ الإعدادات.", "Error al guardar los ajustes."},
    {"SETTINGS SAVED TO INTERNAL HDD.", "تم حفظ الإعدادات على القرص الداخلي.", "Ajustes guardados en el disco interno."},
    {"NO PS3 SCALE UPDATE PACKAGE FOUND ON USB.", "لم يتم العثور على تحديث PS3 SCALE على USB.", "No se encontró una actualización de PS3 SCALE en USB."},
    {"MUSIC COPIED; PLAYBACK NOW USES INTERNAL HDD.", "تم نسخ الموسيقى وأصبح التشغيل من القرص الداخلي.", "Música copiada; ahora se reproduce desde el disco interno."},
    {"MUSIC COPIED TO HDD. RESTART APP TO START AUDIO.", "تم نسخ الموسيقى. أعد تشغيل البرنامج لتشغيلها.", "Música copiada. Reinicia la aplicación para reproducirla."},
    {"MUSIC ALREADY ON HDD; PLAYBACK SWITCHED TO INTERNAL.", "الموسيقى موجودة على القرص؛ تم تحويل التشغيل إليه.", "La música ya está en el disco; reproducción cambiada al interno."},
    {"MUSIC ALREADY ON HDD. RESTART APP IF AUDIO IS STOPPED.", "الموسيقى موجودة على القرص. أعد التشغيل إذا توقف الصوت.", "La música ya está en el disco. Reinicia si no suena."},
    {"COPY FAILED. CHECK USB MUSIC FILE AND HDD SPACE.", "فشل النسخ. تحقق من ملف USB والمساحة المتاحة.", "Error al copiar. Comprueba el USB y el espacio."},
    {"APOLLO SAVE TOOL", "أداة Apollo للحفظ", "Apollo Save Tool"},
    {"STATUS: INSTALLED", "الحالة: مثبت", "Estado: instalado"},
    {"STATUS: NOT DETECTED", "الحالة: غير مكتشف", "Estado: no detectado"},
    {"STATUS: PLUGIN FOUND", "الحالة: الإضافة موجودة", "Estado: plugin encontrado"},
    {"SAVE BACKUP / RESTORE / RESIGN / TROPHY AND LICENSE TOOLS.", "نسخ واستعادة ملفات الحفظ وإدارة الكؤوس والتراخيص.", "Copia/restauración de partidas, trofeos y licencias."},
    {"ARTEMIS PS3", "Artemis PS3", "Artemis PS3"},
    {"TITLE-SPECIFIC CHEAT DATABASE AND ITS OWN RUNTIME PLUGIN.", "قاعدة غش خاصة بكل لعبة مع إضافة تشغيل مستقلة.", "Base de trucos por juego y su propio plugin."},
    {"webMAN MOD", "webMAN MOD", "webMAN MOD"},
    {"GAME MOUNTING / FTP / TEMPERATURE / OPTIONAL PS3MAPI.", "تركيب الألعاب وFTP ودرجات الحرارة وPS3MAPI اختيارياً.", "Montaje de juegos, FTP, temperatura y PS3MAPI opcional."},
    {"SAFE INTEGRATION", "تكامل آمن", "Integración segura"},
    {"PS3 SCALE DOES NOT REPLACE OR MODIFY THESE APPLICATIONS.", "PS3 SCALE لا يستبدل هذه التطبيقات ولا يعدّلها.", "PS3 SCALE no reemplaza ni modifica estas aplicaciones."},
    {"OPEN INSTALLED TOOLS FROM THE XMB GAME MENU.", "افتح الأدوات المثبتة من قائمة الألعاب في XMB.", "Abre las herramientas instaladas desde el menú XMB."},
    {"STATUS UPDATED. OPEN INSTALLED TOOLS FROM XMB.", "تم تحديث الحالة. افتح الأدوات من XMB.", "Estado actualizado. Abre las herramientas desde XMB."},
    {"CROSS: REFRESH TOOL STATUS  |  CIRCLE: BACK", "X: تحديث حالة الأدوات | دائرة: رجوع", "X: actualizar estado | Círculo: volver"},
    {"ABOUT", "حول البرنامج", "Acerca de"},
    {"PS3 / PS2 GAME LIBRARY AND GRAPHICS PROFILE MANAGER.", "إدارة مكتبة ألعاب PS3 وPS2 وملفات تحسين الرسوم.", "Biblioteca PS3/PS2 y gestor de perfiles gráficos."},
    {"TARGET TEST DEVICE: PS3 SLIM / CEX CFW.", "جهاز الاختبار المستهدف: PS3 Slim / CEX CFW.", "Equipo de prueba: PS3 Slim / CEX CFW."},
    {"ALPHA 0.2 - EXPERIMENTAL HOMEBREW SOFTWARE.", "Alpha 0.2 - برنامج منزلي تجريبي.", "Alpha 0.2 - software homebrew experimental."},
    {"SCANNING IS READ-ONLY. ORIGINAL GAME FILES ARE PRESERVED.", "الفحص للقراءة فقط ويحافظ على ملفات الألعاب الأصلية.", "El escaneo es de solo lectura; los archivos originales se conservan."},
    {"NO UNIVERSAL UPSCALER OR FRAME GENERATION IS CLAIMED.", "لا ندّعي وجود رفع دقة شامل أو توليد إطارات.", "No se afirma que exista escalado universal ni generación de fotogramas."},
    {"PROFILES NEED EXACT TITLE / UPDATE MATCHING.", "تتطلب الملفات مطابقة دقيقة لعنوان اللعبة وتحديثها.", "Los perfiles requieren coincidencia exacta de título y actualización."},
    {"CHOOSE LANGUAGE", "اختر اللغة", "Elige el idioma"},
    {"PRESS CROSS TO CONTINUE", "اضغط X للمتابعة", "Pulsa X para continuar"},
    {"STARTING PS3 SCALE", "جارٍ تشغيل PS3 SCALE", "Iniciando PS3 SCALE"},
    {"INITIALIZING GRAPHICS AND CONTROLLER", "تهيئة الرسوم ووحدة التحكم", "Inicializando gráficos y mando"},
    {"CHECKING SETTINGS AND AUDIO", "فحص الإعدادات والصوت", "Comprobando ajustes y audio"},
    {"SCANNING LOCAL GAME LIBRARY", "فحص مكتبة الألعاب المحلية", "Escaneando biblioteca local"},
    {"CHECKING COMPANION TOOLS", "فحص الأدوات المساعدة", "Comprobando herramientas auxiliares"},
    {"READY", "جاهز", "Listo"},
    {"PLEASE WAIT...", "يرجى الانتظار...", "Espera..."},
    {"SEARCH GAMES", "البحث عن الألعاب", "Buscar juegos"},
    {"TYPE TO FILTER THE LIBRARY", "اكتب لتصفية مكتبة الألعاب", "Escribe para filtrar la biblioteca"},
    {"SPACE", "مسافة", "Espacio"},
    {"BACKSPACE", "حذف", "Borrar"},
    {"CLEAR", "مسح", "Limpiar"},
    {"APPLY", "تطبيق", "Aplicar"},
    {"CANCEL", "إلغاء", "Cancelar"},
    {"ARROWS MOVE  |  CROSS SELECT  |  CIRCLE CANCEL", "الأسهم للتنقل | X للاختيار | دائرة للإلغاء", "Flechas mover | X seleccionar | Círculo cancelar"},
    {"UP/DOWN: SELECT  |  CROSS: CONTINUE", "أعلى/أسفل: اختيار | X: متابعة", "Arriba/abajo: elegir | X: continuar"},
    {"STARTING PS3 SCALE", "جارٍ تشغيل PS3 SCALE", "Iniciando PS3 SCALE"},
    {"APP FILE CHECK: OK", "فحص ملفات البرنامج: سليم", "Archivos de la aplicación: correctos"},
    {"APP FILE CHECK: VERIFY INSTALL", "فحص ملفات البرنامج: تحقق من التثبيت", "Archivos de la aplicación: verifica la instalación"},
    {"LOCAL CHECKS ONLY  |  NO GAME FILES ARE MODIFIED", "فحوصات محلية فقط | لا يتم تعديل ملفات الألعاب", "Solo comprobaciones locales | no se modifican juegos"},
    {"CUSTOM KEYBOARD  |  NO PS3 SYSTEM KEYBOARD REQUIRED", "لوحة مفاتيح داخلية | لا حاجة للوحة PS3 الأصلية", "Teclado propio | no se usa el teclado del sistema PS3"},
    {"UP/DOWN: MOVE  CROSS: SELECT  LEFT/RIGHT: ADJUST  CIRCLE: BACK", "أعلى/أسفل: تنقل | X: اختيار | يمين/يسار: تعديل | دائرة: رجوع", "Arriba/abajo: mover | X: elegir | izquierda/derecha: ajustar | círculo: volver"},
    {"NO COVER", "لا توجد صورة غلاف", "Sin carátula"},
    {"CIRCLE: BACK", "دائرة: رجوع", "Círculo: volver"},
    {"UPDATE PKG FOUND. INSTALL VIA PACKAGE MANAGER.", "تم العثور على تحديث PKG. ثبّته عبر مدير الحزم.", "Actualización PKG encontrada. Instálala desde Package Manager."}
};

typedef struct locale_prefix {
    const char *english;
    const char *arabic;
    const char *spanish;
} locale_prefix_t;

static const locale_prefix_t locale_prefixes[] = {
    {"PS3 TITLES: ", "عناوين PS3: ", "Juegos PS3: "},
    {"PS2 TITLES: ", "عناوين PS2: ", "Juegos PS2: "},
    {"TOTAL TITLES: ", "إجمالي الألعاب: ", "Total de juegos: "},
    {"TITLE: ", "العنوان: ", "Título: "},
    {"TYPE: ", "النوع: ", "Tipo: "},
    {"TITLE ID: ", "معرّف اللعبة: ", "ID del juego: "},
    {"PS2 SERIAL: ", "رقم PS2: ", "Serie PS2: "},
    {"PATH: ", "المسار: ", "Ruta: "},
    {"SEARCH: ", "بحث: ", "Buscar: "},
    {"PS2 CONFIG PREVIEW: ", "معاينة إعداد PS2: ", "Vista previa CONFIG PS2: "},
    {"STATUS: ", "الحالة: ", "Estado: "},
    {"AUDIO ENGINE: READY  |  SOURCE: ", "الصوت: جاهز | المصدر: ", "Audio: listo | origen: "},
    {"SCAN COMPLETE: ", "اكتمل الفحص: ", "Escaneo completo: "},
    {"UPDATE PKG FOUND ON USB", "تم العثور على حزمة تحديث على USB", "Paquete de actualización encontrado en USB"}
};

static void copy_string(char *out, size_t cap, const char *src)
{
    size_t n;
    if (!out || cap == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(out, src, n);
    out[n] = '\0';
}

int ps3scale_locale_translate(int language, const char *source,
                              char *output, size_t output_size)
{
    size_t i;
    unsigned ps3, ps2, results;
    if (!source || !output || output_size == 0) return 0;
    if (language != PS3SCALE_LANG_ARABIC &&
        language != PS3SCALE_LANG_SPANISH) {
        copy_string(output, output_size, source);
        return 0;
    }

    for (i = 0; i < sizeof(locale_entries) / sizeof(locale_entries[0]); i++) {
        if (strcmp(source, locale_entries[i].english) == 0) {
            copy_string(output, output_size,
                language == PS3SCALE_LANG_ARABIC ?
                locale_entries[i].arabic : locale_entries[i].spanish);
            return 1;
        }
    }

    if (sscanf(source, "PS3: %u  |  PS2: %u  |  RESULTS: %u",
               &ps3, &ps2, &results) == 3) {
        if (language == PS3SCALE_LANG_ARABIC)
            snprintf(output, output_size, "PS3: %u | PS2: %u | النتائج: %u",
                     ps3, ps2, results);
        else
            snprintf(output, output_size, "PS3: %u | PS2: %u | Resultados: %u",
                     ps3, ps2, results);
        return 1;
    }

    for (i = 0; i < sizeof(locale_prefixes) / sizeof(locale_prefixes[0]); i++) {
        size_t n = strlen(locale_prefixes[i].english);
        if (strncmp(source, locale_prefixes[i].english, n) == 0) {
            const char *prefix = language == PS3SCALE_LANG_ARABIC ?
                locale_prefixes[i].arabic : locale_prefixes[i].spanish;
            snprintf(output, output_size, "%s%s", prefix, source + n);
            return 1;
        }
    }

    copy_string(output, output_size, source);
    return 0;
}

static int utf8_next(const unsigned char *s, size_t length,
                     size_t *offset, uint32_t *codepoint)
{
    unsigned char c;
    if (!s || !offset || !codepoint || *offset >= length) return 0;
    c = s[(*offset)++];
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    if ((c & 0xe0) == 0xc0 && *offset < length) {
        unsigned char c1 = s[(*offset)++];
        *codepoint = ((uint32_t)(c & 0x1f) << 6) |
                     (uint32_t)(c1 & 0x3f);
        return 1;
    }
    if ((c & 0xf0) == 0xe0 && *offset + 1 < length) {
        unsigned char c1 = s[(*offset)++];
        unsigned char c2 = s[(*offset)++];
        *codepoint = ((uint32_t)(c & 0x0f) << 12) |
                     ((uint32_t)(c1 & 0x3f) << 6) |
                     (uint32_t)(c2 & 0x3f);
        return 1;
    }
    if ((c & 0xf8) == 0xf0 && *offset + 2 < length) {
        unsigned char c1 = s[(*offset)++];
        unsigned char c2 = s[(*offset)++];
        unsigned char c3 = s[(*offset)++];
        *codepoint = ((uint32_t)(c & 0x07) << 18) |
                     ((uint32_t)(c1 & 0x3f) << 12) |
                     ((uint32_t)(c2 & 0x3f) << 6) |
                     (uint32_t)(c3 & 0x3f);
        return 1;
    }
    *codepoint = '?';
    return 1;
}

static const ps3scale_arabic_form_t *find_arabic_form(uint32_t cp)
{
    size_t i;
    for (i = 0; i < sizeof(ps3scale_arabic_forms) /
                    sizeof(ps3scale_arabic_forms[0]); i++) {
        if (ps3scale_arabic_forms[i].codepoint == cp)
            return &ps3scale_arabic_forms[i];
    }
    return NULL;
}

static uint8_t unicode_to_glyph(uint32_t cp)
{
    size_t i;
    const ps3scale_arabic_form_t *form = find_arabic_form(cp);
    if (form) return form->isolated ? form->isolated : (uint8_t)'?';
    for (i = 0; i < sizeof(ps3scale_extra_glyphs) /
                    sizeof(ps3scale_extra_glyphs[0]); i++) {
        if (ps3scale_extra_glyphs[i].codepoint == cp)
            return ps3scale_extra_glyphs[i].glyph;
    }
    /* Uppercase Spanish accents share the matching lowercase glyph. */
    if (cp == 0x00c1) return 246;
    if (cp == 0x00c9) return 247;
    if (cp == 0x00cd) return 248;
    if (cp == 0x00d3) return 249;
    if (cp == 0x00da) return 250;
    if (cp == 0x00d1) return 251;
    if (cp == 0x00dc) return 252;
    if (cp < 128) return (uint8_t)cp;
    if (cp == 0x2019 || cp == 0x2018) return '\'';
    if (cp == 0x2013 || cp == 0x2014) return '-';
    return (uint8_t)'?';
}

static int is_arabic_codepoint(uint32_t cp)
{
    return (cp >= 0x0600 && cp <= 0x06ff);
}

static int strong_direction(uint32_t cp)
{
    if (is_arabic_codepoint(cp)) return 1;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
        (cp >= '0' && cp <= '9') ||
        (cp >= 0x00c0 && cp <= 0x02af))
        return 0;
    return -1;
}

static uint8_t shaped_arabic_glyph(const uint32_t *cps, int index,
                                   int run_start, int run_end)
{
    const ps3scale_arabic_form_t *current =
        find_arabic_form(cps[index]);
    const ps3scale_arabic_form_t *previous = NULL;
    const ps3scale_arabic_form_t *next = NULL;
    int join_previous = 0, join_next = 0;

    if (!current) return unicode_to_glyph(cps[index]);
    if (index > run_start)
        previous = find_arabic_form(cps[index - 1]);
    if (index + 1 < run_end)
        next = find_arabic_form(cps[index + 1]);

    if (previous && previous->initial &&
        (current->final_form || current->medial))
        join_previous = 1;
    if (next && next->final_form &&
        (current->initial || current->medial))
        join_next = 1;

    if (join_previous && join_next && current->medial)
        return current->medial;
    if (join_previous && current->final_form)
        return current->final_form;
    if (join_next && current->initial)
        return current->initial;
    return current->isolated ? current->isolated : (uint8_t)'?';
}

typedef struct text_run {
    int start;
    int end;
    int direction;
} text_run_t;

int ps3scale_locale_to_glyphs(const char *source, char *output,
                              size_t output_size)
{
    uint32_t cps[256];
    int dirs[256];
    text_run_t runs[256];
    int count = 0, run_count = 0, base_direction = 0;
    size_t offset = 0, source_length, out = 0;
    int i, have_strong = 0;

    if (!source || !output || output_size == 0) return -1;
    source_length = strlen(source);
    while (offset < source_length && count < 256) {
        uint32_t cp = '?';
        if (!utf8_next((const unsigned char *)source, source_length,
                       &offset, &cp)) break;
        cps[count++] = cp;
        if (!have_strong && strong_direction(cp) >= 0) {
            base_direction = strong_direction(cp);
            have_strong = 1;
        }
    }

    if (!have_strong) base_direction = 0;
    for (i = 0; i < count; i++) {
        int direction = strong_direction(cps[i]);
        if (direction >= 0) dirs[i] = direction;
        else dirs[i] = (i > 0) ? dirs[i - 1] : base_direction;
    }

    for (i = 0; i < count; ) {
        int start = i, direction = dirs[i];
        while (i < count && dirs[i] == direction) i++;
        runs[run_count].start = start;
        runs[run_count].end = i;
        runs[run_count].direction = direction;
        run_count++;
    }

    if (base_direction == 1) {
        int r;
        for (r = run_count - 1; r >= 0; r--) {
            text_run_t run = runs[r];
            if (run.direction == 1) {
                int j;
                for (j = run.end - 1; j >= run.start; j--) {
                    uint8_t glyph = shaped_arabic_glyph(cps, j,
                                                       run.start, run.end);
                    if (out + 1 >= output_size) goto finished;
                    output[out++] = (char)glyph;
                }
            } else {
                int j;
                for (j = run.start; j < run.end; j++) {
                    if (out + 1 >= output_size) goto finished;
                    output[out++] = (char)unicode_to_glyph(cps[j]);
                }
            }
        }
    } else {
        int r;
        for (r = 0; r < run_count; r++) {
            text_run_t run = runs[r];
            if (run.direction == 1) {
                int j;
                for (j = run.end - 1; j >= run.start; j--) {
                    uint8_t glyph = shaped_arabic_glyph(cps, j,
                                                       run.start, run.end);
                    if (out + 1 >= output_size) goto finished;
                    output[out++] = (char)glyph;
                }
            } else {
                int j;
                for (j = run.start; j < run.end; j++) {
                    if (out + 1 >= output_size) goto finished;
                    output[out++] = (char)unicode_to_glyph(cps[j]);
                }
            }
        }
    }

finished:
    output[out] = '\0';
    return (int)out;
}

int ps3scale_locale_glyph_font_index(unsigned char glyph)
{
    if (glyph >= 128u && glyph <= 244u) return 1; /* Arabic TTF font */
    if (glyph >= 246u && glyph <= 254u) return 2; /* Spanish bitmap extras */
    return 0; /* Latin ASCII font */
}

int ps3scale_locale_is_arabic_glyph(unsigned char glyph)
{
    size_t i;
    for (i = 0; i < sizeof(ps3scale_arabic_forms) /
                    sizeof(ps3scale_arabic_forms[0]); i++) {
        const ps3scale_arabic_form_t *form = &ps3scale_arabic_forms[i];
        if (form->isolated == glyph ||
            (form->final_form && form->final_form == glyph) ||
            (form->initial && form->initial == glyph) ||
            (form->medial && form->medial == glyph))
            return 1;
    }
    return 0;
}

int ps3scale_locale_glyphs_joined(unsigned char left_glyph,
                                  unsigned char right_glyph)
{
    const ps3scale_arabic_form_t *left_form = NULL;
    const ps3scale_arabic_form_t *right_form = NULL;
    size_t i;

    for (i = 0; i < sizeof(ps3scale_arabic_forms) /
                    sizeof(ps3scale_arabic_forms[0]); i++) {
        const ps3scale_arabic_form_t *form = &ps3scale_arabic_forms[i];
        if ((form->final_form && form->final_form == left_glyph) ||
            (form->medial && form->medial == left_glyph))
            left_form = form;
        if ((form->initial && form->initial == right_glyph) ||
            (form->medial && form->medial == right_glyph))
            right_form = form;
    }

    return left_form != NULL && right_form != NULL;
}

const char *ps3scale_locale_language_name(int language)
{
    switch (language) {
        case PS3SCALE_LANG_ARABIC: return "العربية";
        case PS3SCALE_LANG_SPANISH: return "ESPAÑOL";
        default: return "ENGLISH";
    }
}

const char *ps3scale_locale_keyboard_key(int language, int index)
{
    static const char *english[40] = {
        "1","2","3","4","5","6","7","8","9","0",
        "Q","W","E","R","T","Y","U","I","O","P",
        "A","S","D","F","G","H","J","K","L","-",
        "Z","X","C","V","B","N","M",".",",","_"
    };
    static const char *spanish[40] = {
        "1","2","3","4","5","6","7","8","9","0",
        "Q","W","E","R","T","Y","U","I","O","P",
        "A","S","D","F","G","H","J","K","L","-",
        "Z","X","C","V","B","N","M","ñ",".","_"
    };
    static const char *arabic[40] = {
        "1","2","3","4","5","6","7","8","9","0",
        "ا","ب","ت","ث","ج","ح","خ","د","ذ","ر",
        "ز","س","ش","ص","ض","ط","ظ","ع","غ","ف",
        "ق","ك","ل","م","ن","ه","و","ي","ة","ى"
    };
    if (index < 0 || index >= 40) return "";
    if (language == PS3SCALE_LANG_ARABIC) return arabic[index];
    if (language == PS3SCALE_LANG_SPANISH) return spanish[index];
    return english[index];
}
