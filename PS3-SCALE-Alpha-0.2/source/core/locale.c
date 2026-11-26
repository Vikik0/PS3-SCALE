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
    {"UPDATE PKG FOUND. INSTALL VIA PACKAGE MANAGER.", "تم العثور على تحديث PKG. ثبّته عبر مدير الحزم.", "Actualización PKG encontrada. Instálala desde Package Manager."},
    {"HARDWARE INFORMATION", "معلومات الجهاز", "Información del hardware"},
    {"DETECTED MODEL:", "الموديل المكتشف:", "Modelo detectado:"},
    {"FIRMWARE:", "إصدار النظام:", "Firmware:"},
    {"PLATFORM ID:", "معرّف المنصة:", "ID de plataforma:"},
    {"CONSOLE TYPE:", "نوع الجهاز:", "Tipo de consola:"},
    {"HARDWARE INFO UNAVAILABLE.", "معلومات الجهاز غير متاحة.", "Información de hardware no disponible."},
    {"TARGET HARDWARE: PS3 FAT / SLIM / SUPER SLIM.", "الأجهزة المستهدفة: PS3 FAT وSLIM وSUPER SLIM.", "Hardware objetivo: PS3 FAT / SLIM / SUPER SLIM."},

    {"ONLINE GRAPHICS DATABASE", "قاعدة بيانات تحسين الرسوم", "Base de datos de mejoras gráficas"},
    {"MATCHED GAMES", "الألعاب المتطابقة", "Juegos coincidentes"},
    {"PROFILE DETAILS", "تفاصيل الملف", "Detalles del perfil"},
    {"DATABASE STATUS", "حالة قاعدة البيانات", "Estado de la base de datos"},
    {"NO EXACT MATCHES IN YOUR PS3 LIBRARY.", "لا توجد مطابقات دقيقة في مكتبة PS3.", "No hay coincidencias exactas en tu biblioteca PS3."},
    {"THE DATABASE IS VERSION-SPECIFIC.", "قاعدة البيانات مرتبطة بإصدار اللعبة.", "La base de datos depende de la versión."},
    {"RESCAN GAMES AFTER INSTALLING UPDATES.", "أعد فحص الألعاب بعد تثبيت التحديثات.", "Vuelve a escanear tras instalar actualizaciones."},
    {"TRUST: COMMUNITY-REPORTED", "الموثوقية: بلاغات مجتمعية", "Confianza: reportado por la comunidad"},
    {"AUTO APPLY: AVAILABLE AFTER CONFIRMATION", "التطبيق التلقائي: متاح بعد التأكيد", "Aplicación automática: disponible tras confirmar"},
    {"AUTO APPLY: DISABLED FOR MULTI-WRITE SAFETY", "التطبيق التلقائي: متوقف لأمان التعديلات المتعددة", "Aplicación automática: desactivada por seguridad"},
    {"MATCH: EXACT TITLE ID + APP_VER", "المطابقة: معرّف اللعبة وإصدارها", "Coincidencia: ID y versión exactos"},
    {"SOURCE: COMMUNITY MAPI PATCH NOTES", "المصدر: تعديلات MAPI المجتمعية", "Fuente: parches MAPI comunitarios"},
    {"UP/DOWN: SELECT  CROSS: OPEN  SQUARE: CACHE  TRIANGLE: ALL  START: SYNC  CIRCLE: BACK", "أعلى/أسفل: اختيار  X: فتح  مربع: حفظ  مثلث: الكل  START: مزامنة  دائرة: رجوع", "Arriba/abajo: elegir  X: abrir  Cuadrado: guardar  Triángulo: todos  START: sincronizar  Círculo: volver"},
    {"LOCAL SCAN + OPTIONAL ONLINE PROFILE SYNC", "فحص محلي مع مزامنة اختيارية للملفات", "Escaneo local y sincronización opcional"},
    {"NO EXACT ONLINE PROFILE FOR THIS UPDATE.", "لا يوجد ملف أونلاين مطابق لهذا التحديث.", "No hay perfil en línea exacto para esta actualización."},
    {"TRUST: COMMUNITY-REPORTED - REVIEW BEFORE USE.", "المصدر مجتمعي؛ راجع التعديل قبل استخدامه.", "Origen comunitario; revisa el parche antes de usarlo."},
    {"TITLE ID AND APP VERSION MUST BOTH MATCH.", "يجب تطابق معرّف اللعبة وإصدارها معاً.", "Deben coincidir el ID y la versión del juego."},
    {"OPEN ONLINE DATABASE TO CHECK SUPPORTED PROFILES.", "افتح قاعدة البيانات لمعرفة الملفات المدعومة.", "Abre la base de datos para ver perfiles compatibles."},
    {"COMMUNITY PATCH: TRIANGLE CONFIRMS INSTALL; CIRCLE CANCELS.", "تعديل مجتمعي: مثلث للتثبيت ودائرة للإلغاء.", "Parche comunitario: Triángulo instala; Círculo cancela."},
    {"START GAME", "تشغيل اللعبة", "Iniciar juego"},
    {"EXACT ONLINE PROFILE FOUND. REVIEW BEFORE INSTALL.", "تم العثور على ملف مطابق؛ راجعه قبل التثبيت.", "Perfil exacto encontrado; revísalo antes de instalar."},
    {"NO EXACT ONLINE PROFILE FOR THIS GAME UPDATE.", "لا يوجد ملف أونلاين مطابق لهذا التحديث.", "No hay perfil en línea exacto para esta actualización."},
    {"MULTI-WRITE PROFILE: AUTO APPLY DISABLED FOR SAFETY.", "الملف يتطلب تعديلات متعددة؛ التطبيق التلقائي متوقف للأمان.", "Perfil con varias escrituras; aplicación automática desactivada."},
    {"ONE-LAUNCH PROFILE SCRIPT REMOVED FROM WEBMAN.", "تمت إزالة سكربت التحسين لمرة واحدة من webMAN.", "Script de perfil de un solo inicio eliminado de webMAN."},
    {"COULD NOT REMOVE THE PS3 SCALE PROFILE SCRIPT.", "تعذرت إزالة سكربت PS3 SCALE.", "No se pudo eliminar el script de PS3 SCALE."},
    {"A USER WEBMAN SCRIPT EXISTS. IT WAS NOT OVERWRITTEN.", "يوجد سكربت webMAN خاص بك؛ لم يتم استبداله.", "Ya existe un script de webMAN; no se sobrescribió."},
    {"COMMUNITY PATCH. TRIANGLE TO ARM ONE-LAUNCH PROFILE.", "تعديل مجتمعي؛ اضغط مثلث لتجهيز التحسين للتشغيل مرة واحدة.", "Parche comunitario; pulsa Triángulo para preparar el perfil de un solo inicio."},
    {"PROFILE INSTALL CANCELLED.", "تم إلغاء تثبيت الملف.", "Instalación del perfil cancelada."},
    {"STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING.", "سكربت التحسين قديم؛ أزِله قبل تشغيل اللعبة.", "Script de perfil obsoleto. Elimínalo antes de iniciar."},
    {"PS3MAPI IS NOT ACTIVE. ENABLE IT IN WEBMAN SETTINGS.", "PS3MAPI غير نشط؛ فعّله من إعدادات webMAN.", "PS3MAPI no está activo; actívalo en webMAN."},
    {"ONE-LAUNCH PROFILE WILL CHECK THE EXACT ADDRESS AND ORIGINAL BYTES.", "سيُفحص العنوان والبايتات الأصلية قبل تطبيق التحسين لمرة واحدة.", "El perfil comprobará la dirección exacta y los bytes originales al iniciar."},
    {"EXACT PROFILE FOUND, BUT ONE-LAUNCH APPLY IS NOT ARMED. STARTING UNMODIFIED.", "عُثر على ملف مطابق، لكن التحسين لم يُجهّز؛ ستبدأ اللعبة دون تعديل.", "Perfil exacto encontrado, pero no está preparado; el juego iniciará sin cambios."},
    {"NO EXACT ONLINE PROFILE. STARTING UNMODIFIED GAME.", "لا يوجد ملف أونلاين مطابق؛ ستبدأ اللعبة دون تعديل.", "No hay perfil exacto; el juego se iniciará sin cambios."},
    {"ENABLE WEBMAN PS3MAPI SERVER BEFORE INSTALLING.", "فعّل خادم PS3MAPI في webMAN قبل التثبيت.", "Activa el servidor PS3MAPI de webMAN antes de instalar."},
    {"WAIT FOR ONLINE DATABASE SYNC TO FINISH.", "انتظر حتى تنتهي مزامنة قاعدة البيانات.", "Espera a que termine la sincronización de la base de datos."},
    {"WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING.", "انتظر انتهاء مزامنة قاعدة البيانات قبل التشغيل.", "Espera a que termine la sincronización antes de iniciar."},
    {"MOUNT THE EXACT GAME VERSION BEFORE ARMING THIS PROFILE.", "ركّب إصدار اللعبة المطابق قبل تجهيز ملف التحسين.", "Monta la versión exacta del juego antes de preparar el perfil."},
    {"MANUAL-ONLY PROFILE. ONE-LAUNCH APPLY IS NOT SUPPORTED.", "هذا الملف يدوي فقط؛ التطبيق لمرة واحدة غير مدعوم له.", "Perfil manual; la aplicación de un solo inicio no está disponible."},
    {"TRUST: COMMUNITY-REPORTED / MANUAL ONLY", "الثقة: تقرير مجتمعي / يدوي فقط", "Confianza: informe comunitario / solo manual"},
    {"MANUAL ONLY: RE-APPLY REQUIRED IN GAME", "يدوي فقط: يجب إعادة التطبيق داخل اللعبة", "Solo manual: hay que volver a aplicarlo dentro del juego"},
    {"NO PROFILE FOR THIS TITLE. GAME WILL START UNMODIFIED.", "لا يوجد ملف تحسين لهذه اللعبة؛ ستعمل دون تعديلات.", "No hay perfil para este título; el juego iniciará sin modificaciones."},
    {"STARTING GAME WITH ONE-LAUNCH PROFILE.", "جارٍ تشغيل اللعبة مع ملف تحسين لمرة واحدة.", "Iniciando el juego con un perfil de una sola ejecución."},
    {"PROFILE NOT ARMED. STARTING GAME UNMODIFIED.", "لم يتم تجهيز ملف التحسين؛ ستعمل اللعبة دون تعديلات.", "Perfil no activado; el juego iniciará sin modificaciones."},
    {"NO EXACT PROFILE. STARTING GAME UNMODIFIED.", "لا يوجد ملف مطابق؛ ستعمل اللعبة دون تحسينات.", "No hay perfil exacto; el juego iniciará sin modificaciones."},
    {"WEBMAN ACCEPTED START REQUEST.", "قبل webMAN طلب تشغيل اللعبة.", "webMAN aceptó la solicitud de inicio."},
    {"PS3MAPI NOT ACTIVE. PROFILE WAS NOT APPLIED.", "PS3MAPI غير مفعّل؛ لم يُطبّق ملف التحسين.", "PS3MAPI no está activo; no se aplicó el perfil."},
    {"WEBMAN START FAILED. APP STILL RESPONSIVE.", "فشل تشغيل اللعبة عبر webMAN؛ التطبيق ما زال يستجيب.", "webMAN no pudo iniciar el juego; la aplicación sigue respondiendo."},
    {"GAME UPDATE CHANGED. RESCAN BEFORE APPLYING PROFILE.", "تغيّر إصدار اللعبة؛ أعد الفحص قبل تجهيز التحسين.", "Cambió la actualización; vuelve a escanear antes de activar el perfil."},
    {"GAME UPDATE CHANGED. RESCAN BEFORE STARTING PROFILE.", "تغيّر إصدار اللعبة؛ أعد الفحص قبل تشغيل التحسين.", "Cambió la actualización; vuelve a escanear antes de iniciar el perfil."},
    {"MOUNT THE EXACT GAME VERSION BEFORE STARTING PROFILE.", "ركّب إصدار اللعبة المطابق قبل تشغيل ملف التحسين.", "Monta la versión exacta antes de iniciar el perfil."},
    {"WAIT FOR ONLINE DATABASE OR GAME ACTION TO FINISH.", "انتظر انتهاء مزامنة قاعدة البيانات أو عملية اللعبة.", "Espera a que termine la sincronización o la acción del juego."},
    {"GAME START REQUEST IS ALREADY RUNNING.", "طلب تشغيل اللعبة قيد التنفيذ بالفعل.", "La solicitud de inicio ya está en curso."},
    {"START UNMODIFIED", "تشغيل دون تحسين", "Iniciar sin modificaciones"},
    {"START WITH PROFILE", "تشغيل مع التحسين", "Iniciar con perfil"},
    {"STARTING...", "جارٍ التشغيل...", "Iniciando..."},
    {"COULD NOT START GAME LAUNCH WORKER. APP IS READY.", "تعذر بدء مهمة التشغيل؛ التطبيق جاهز.", "No se pudo iniciar la tarea; la aplicación está lista."},
    {"CHECKING WEBMAN AND ARMING PROFILE.", "جارٍ التحقق من webMAN وتجهيز ملف التحسين.", "Comprobando webMAN y activando el perfil."},
    {"ONE-LAUNCH PROFILE ARMED. START THIS GAME.", "تم تجهيز ملف التحسين لمرة واحدة؛ شغّل هذه اللعبة.", "Perfil de una sola ejecución activado; inicia este juego."},
    {"WEBMAN MOD NOT RESPONDING. PROFILE NOT ARMED.", "webMAN MOD لا يستجيب؛ لم يتم تجهيز ملف التحسين.", "webMAN MOD no responde; el perfil no se activó."},
    {"PS3MAPI NOT ACTIVE. PROFILE NOT ARMED.", "PS3MAPI غير مفعّل؛ لم يتم تجهيز ملف التحسين.", "PS3MAPI no está activo; el perfil no se activó."},
    {"COULD NOT START PROFILE WORKER. NO PATCH WAS ARMED.", "تعذر بدء مهمة التجهيز؛ لم يتم تفعيل أي تعديل.", "No se pudo iniciar la tarea; no se activó ningún parche."},
    {"WAIT FOR PROFILE ARMING TO FINISH.", "انتظر حتى ينتهي تجهيز ملف التحسين.", "Espera a que termine la activación del perfil."},
    {"GAME OR PROFILE ACTION IS ALREADY RUNNING.", "هناك عملية تشغيل أو تجهيز ملف تحسين قيد التنفيذ.", "Ya hay una acción de juego o perfil en curso."},
    {"ARMING PROFILE...", "جارٍ تجهيز التحسين...", "Activando perfil..."},


    {"SETTINGS RESTORED TO DEFAULTS.", "تمت استعادة الإعدادات الافتراضية.", "Se restauraron los ajustes predeterminados."},

    {"ONE-LAUNCH PROFILE ARMED. START THE MOUNTED GAME.", "تم تجهيز ملف التحسين للتشغيل مرة واحدة؛ شغّل اللعبة المركّبة.", "Perfil de un solo inicio preparado; inicia el juego montado."},
    {"MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY.", "الملف يتطلب تعديلات متعددة؛ التطبيق التلقائي متوقف للأمان.", "Perfil de varias escrituras; aplicación automática desactivada."},
    {"PROFILE SCRIPT INSTALL FAILED. NO GAME FILES CHANGED.", "فشل تثبيت السكربت؛ لم تتغير ملفات اللعبة.", "Falló el script; no se modificaron archivos del juego."},
    {"PROFILE DATA SAVED TO INTERNAL HDD. NOT APPLIED YET.", "تم حفظ بيانات الملف على القرص الداخلي ولم تُطبّق بعد.", "Perfil guardado en HDD; todavía no se ha aplicado."},
    {"COULD NOT SAVE PROFILE DATA TO INTERNAL HDD.", "تعذر حفظ بيانات الملف على القرص الداخلي.", "No se pudo guardar el perfil en el HDD."},
    {"NO EXACT TITLE ID / APP VERSION MATCH IS SELECTED.", "لا يوجد تطابق دقيق لمعرّف اللعبة وإصدارها.", "No hay coincidencia exacta de ID y versión seleccionada."},
    {"NO EXACT MATCH IS SELECTED.", "لم يتم تحديد تطابق دقيق.", "No hay una coincidencia exacta seleccionada."},
    {"NO ONLINE DATABASE IS LOADED.", "لم يتم تحميل قاعدة بيانات أونلاين.", "No se ha cargado la base de datos en línea."},
    {"DATABASE SYNC IS ALREADY RUNNING.", "مزامنة قاعدة البيانات قيد التشغيل بالفعل.", "La sincronización ya está en curso."},
    {"DATABASE UPDATED. EXACT GAME MATCHES ARE READY.", "تم تحديث قاعدة البيانات وأصبحت المطابقات جاهزة.", "Base de datos actualizada; coincidencias listas."},
    {"UPDATE FAILED. YOUR LAST VALID DATABASE IS STILL AVAILABLE.", "فشل التحديث؛ ما زالت قاعدة البيانات السابقة متاحة.", "Falló la actualización; se conserva la base anterior."},
    {"CONNECT THE PS3 TO THE INTERNET AND PRESS START TO RETRY.", "صل جهاز PS3 بالإنترنت واضغط START لإعادة المحاولة.", "Conecta la PS3 a Internet y pulsa START para reintentar."},
    {"NO VALID LOCAL DATABASE", "لا توجد قاعدة بيانات محلية صالحة", "No hay una base de datos local válida"},
    {"BUNDLED / CACHED DATABASE READY", "قاعدة البيانات المرفقة / المحفوظة جاهزة", "Base de datos incluida / en caché lista"},
    {"CONNECTING TO GITHUB DATABASE", "جارٍ الاتصال بقاعدة بيانات GitHub", "Conectando con la base de datos de GitHub"},
    {"ONLINE DATABASE UPDATED", "تم تحديث قاعدة البيانات عبر الإنترنت", "Base de datos en línea actualizada"},
    {"UPDATE FAILED - USING CACHED DATABASE", "فشل التحديث؛ سيتم استخدام النسخة المحفوظة", "Falló la actualización; se usa la copia guardada"},
    {"OFFLINE - NO VALID DATABASE", "غير متصل - لا توجد قاعدة بيانات صالحة", "Sin conexión - no hay base de datos válida"},
    {"ONLINE THREAD COULD NOT START", "تعذر بدء مهمة الاتصال بالإنترنت", "No se pudo iniciar la tarea en línea"},
    {"SELECT A MATCHED GAME OR PRESS START TO SYNC.", "اختر لعبة متطابقة أو اضغط START للمزامنة.", "Elige un juego coincidente o pulsa START para sincronizar."},
    {"ONE-LAUNCH PROFILE: ARMED", "تحسين لمرة واحدة: جاهز", "Perfil de un solo inicio: preparado"},
    {"ONE-LAUNCH PROFILE: NOT ARMED", "تحسين لمرة واحدة: غير مجهّز", "Perfil de un solo inicio: no preparado"},
    {"PROFILE SCRIPT: STALE - REMOVE", "سكربت التحسين: قديم - أزِله", "Script de perfil: obsoleto - eliminar"},
    {"PROFILE SCRIPT: USER-OWNED - NOT TOUCHED", "سكربت المستخدم: لن يغيّره PS3 SCALE", "Script del usuario: PS3 SCALE no lo modifica"},
    {"ONE-LAUNCH APPLY: AVAILABLE AFTER CONFIRMATION", "التطبيق لمرة واحدة: متاح بعد التأكيد", "Aplicación de un solo inicio: disponible tras confirmar"},
    {"OLD PROFILE SCRIPT DOES NOT MATCH THIS CATALOG.", "سكربت التحسين القديم لا يطابق قاعدة البيانات.", "El script de perfil antiguo no coincide con esta base de datos."},
    {"REMOVE IT BEFORE STARTING THIS GAME.", "أزِله قبل تشغيل هذه اللعبة.", "Elimínalo antes de iniciar este juego."},
    {"NO EXACT PROFILE IS AVAILABLE FOR THIS UPDATE.", "لا يوجد ملف مطابق لهذا التحديث.", "No hay un perfil exacto para esta actualización."},
    {"A USER WEBMAN SCRIPT EXISTS FOR THIS TITLE.", "يوجد سكربت webMAN خاص بك لهذه اللعبة.", "Existe un script de webMAN del usuario para este título."},
    {"PS3 SCALE WILL NOT MODIFY OR REMOVE IT.", "لن يغيّره PS3 SCALE أو يحذفه.", "PS3 SCALE no lo modificará ni lo eliminará."},
    {"NO EXACT ONLINE PROFILE IS AVAILABLE.", "لا يوجد ملف تحسين مطابق عبر الإنترنت.", "No hay un perfil en línea exacto disponible."},
    {"Help & Support", "المساعدة والدعم", "Ayuda y soporte"},
    {"A quick guide to the main PS3 SCALE features", "دليل سريع لأهم ميزات PS3 SCALE", "Guía rápida de las funciones principales de PS3 SCALE"},
    {"GETTING STARTED  /  FIVE SIMPLE STEPS", "البدء / خمس خطوات بسيطة", "INICIO / CINCO PASOS SENCILLOS"},
    {"Scan Games", "فحص الألعاب", "Buscar juegos"},
    {"Select a Game", "اختيار لعبة", "Seleccionar un juego"},
    {"Review Match", "مراجعة المطابقة", "Revisar coincidencia"},
    {"Save Data Safely", "حفظ البيانات بأمان", "Guardar datos de forma segura"},
    {"Start Game", "تشغيل اللعبة", "Iniciar juego"},
    {"Find PS3 / PS2 titles.", "اعثر على ألعاب PS3 وPS2.", "Busca títulos de PS3 y PS2."},
    {"Choose a local title.", "اختر لعبة من المكتبة المحلية.", "Elige un título local."},
    {"Check ID and version.", "تحقق من المعرّف والإصدار.", "Comprueba el ID y la versión."},
    {"Save NCL / TXT data.", "احفظ بيانات NCL وTXT.", "Guarda los datos NCL y TXT."},
    {"Launch when ready.", "شغّل اللعبة عندما تكون جاهزاً.", "Inicia el juego cuando esté listo."},
    {"Report an Issue", "الإبلاغ عن مشكلة", "Informar de un problema"},
    {"Send Feedback", "إرسال ملاحظات", "Enviar comentarios"},
    {"Retry Pending", "إعادة إرسال المعلّق", "Reintentar pendientes"},
    {"Support & Contact", "الدعم والتواصل", "Soporte y contacto"},
    {"Community files are saved as data only; they are never auto-executed.", "تُحفظ ملفات المجتمع كبيانات فقط ولا تُنفّذ تلقائياً.", "Los archivos comunitarios se guardan como datos y nunca se ejecutan automáticamente."},
    {"UP / DOWN: CHOOSE SUPPORT ACTION   |   CROSS: OPEN   |   CIRCLE: BACK", "أعلى/أسفل: اختيار إجراء الدعم | X: فتح | دائرة: رجوع", "ARRIBA/ABAJO: ELEGIR SOPORTE | X: ABRIR | CÍRCULO: VOLVER"},
    {"Send a note from PS3 SCALE", "أرسل ملاحظة من PS3 SCALE", "Envía un mensaje desde PS3 SCALE"},
    {"Report Title", "عنوان البلاغ", "Título del informe"},
    {"Problem Details", "تفاصيل المشكلة", "Detalles del problema"},
    {"Submit Report", "إرسال البلاغ", "Enviar informe"},
    {"Press CROSS to enter a title", "اضغط X لإدخال العنوان", "Pulsa X para escribir un título"},
    {"Press CROSS to describe the issue", "اضغط X لوصف المشكلة", "Pulsa X para describir el problema"},
    {"Save locally; send online if the service is configured", "يُحفظ محلياً ويُرسل عبر الإنترنت إذا كانت الخدمة مهيأة", "Se guarda localmente y se envía en línea si el servicio está configurado"},
    {"Model, firmware, platform and app version are added automatically.", "يُضاف طراز الجهاز والنظام والمنصة وإصدار التطبيق تلقائياً.", "El modelo, firmware, plataforma y versión de la aplicación se añaden automáticamente."},
    {"Never include PSN IDs, passwords or account tokens.", "لا تكتب بيانات PSN أو كلمات المرور أو رموز الحساب.", "No incluyas datos de PSN, contraseñas ni tokens de cuenta."},
    {"UP / DOWN: MOVE   |   CROSS: EDIT / SAVE   |   CIRCLE: BACK", "أعلى/أسفل: تنقل | X: تعديل/حفظ | دائرة: رجوع", "ARRIBA/ABAJO: MOVER | X: EDITAR/GUARDAR | CÍRCULO: VOLVER"},
    {"LOCAL OUTBOX READY.", "قائمة البلاغات المحلية جاهزة.", "Bandeja local preparada."},
    {"SAVE REPORT LOCALLY FIRST.", "احفظ البلاغ محلياً أولاً.", "Guarda primero el informe localmente."},
    {"REPORT ALREADY SENT ONLINE.", "تم إرسال البلاغ عبر الإنترنت مسبقاً.", "El informe ya se envió en línea."},
    {"ANOTHER REPORT IS SENDING.", "يجري إرسال بلاغ آخر.", "Se está enviando otro informe."},
    {"LOCAL SAVE OK; ONLINE OFF.", "تم الحفظ محلياً؛ الإرسال عبر الإنترنت غير مفعّل.", "Guardado local; envío en línea desactivado."},
    {"LOCAL SAVE OK; NETWORK BUSY. PRESS CROSS TO RETRY.", "تم الحفظ محلياً؛ الشبكة مشغولة. اضغط X للمحاولة مجدداً.", "Guardado local; red ocupada. Pulsa X para reintentar."},
    {"LOCAL SAVE OK; REPORT FORMAT INVALID.", "تم الحفظ محلياً؛ صيغة البلاغ غير صالحة.", "Guardado local; formato del informe no válido."},
    {"REPORT QUEUED FOR ONLINE SEND.", "تم وضع البلاغ في قائمة الإرسال.", "Informe añadido a la cola de envío."},
    {"LOCAL SAVE OK; SEND START FAILED. PRESS CROSS TO RETRY.", "تم الحفظ محلياً؛ تعذر بدء الإرسال. اضغط X للمحاولة مجدداً.", "Guardado local; no se pudo iniciar el envío. Pulsa X para reintentar."},
    {"REPORT SENT ONLINE.", "تم إرسال البلاغ عبر الإنترنت.", "Informe enviado en línea."},
    {"PENDING REPORT SENT ONLINE.", "تم إرسال البلاغ المعلّق عبر الإنترنت.", "Informe pendiente enviado en línea."},
    {"SERVER LIMIT REACHED; TRY TOMORROW.", "تم بلوغ حد الخادم؛ حاول غداً.", "Límite del servidor alcanzado; inténtalo mañana."},
    {"SERVER TEMPORARILY UNAVAILABLE; RETRY.", "الخادم غير متاح مؤقتاً؛ أعد المحاولة.", "Servidor temporalmente no disponible; reintenta."},
    {"SERVER REJECTED REPORT; CHECK DETAILS.", "رفض الخادم البلاغ؛ تحقق من التفاصيل.", "El servidor rechazó el informe; revisa los detalles."},
    {"LOCAL SAVE OK; SEND FAILED. PRESS CROSS TO RETRY.", "تم الحفظ محلياً؛ فشل الإرسال. اضغط X للمحاولة مجدداً.", "Guardado local; falló el envío. Pulsa X para reintentar."},
    {"PENDING REPORT FAILED; TRY AGAIN LATER.", "فشل إرسال البلاغ المعلّق؛ حاول لاحقاً.", "Falló el envío del informe pendiente; inténtalo más tarde."},
    {"NETWORK BUSY; TRY PENDING REPORTS AGAIN.", "الشبكة مشغولة؛ أعد محاولة إرسال البلاغات المعلّقة.", "Red ocupada; reintenta los informes pendientes."},
    {"NO PENDING REPORTS.", "لا توجد بلاغات معلّقة.", "No hay informes pendientes."},
    {"PENDING REPORT PATH INVALID.", "مسار البلاغ المعلّق غير صالح.", "La ruta del informe pendiente no es válida."},
    {"PENDING REPORT COULD NOT BE OPENED.", "تعذر فتح البلاغ المعلّق.", "No se pudo abrir el informe pendiente."},
    {"PENDING REPORT IS TOO LARGE OR DAMAGED.", "البلاغ المعلّق كبير جداً أو تالف.", "El informe pendiente es demasiado grande o está dañado."},
    {"PENDING REPORT IS NOT VALID JSON.", "البلاغ المعلّق ليس بصيغة JSON صحيحة.", "El informe pendiente no es JSON válido."},
    {"PENDING REPORT ID DOES NOT MATCH FILE.", "معرّف البلاغ المعلّق لا يطابق الملف.", "El ID del informe pendiente no coincide con el archivo."},
    {"ADD TITLE + DETAILS.", "أدخل العنوان والتفاصيل.", "Añade título y detalles."},
    {"REPORT TOO LONG OR INVALID.", "البلاغ طويل جداً أو غير صالح.", "El informe es demasiado largo o no es válido."},
    {"REPORT SAVE FAILED; PREVIOUS REPORTS KEPT.", "فشل حفظ البلاغ؛ البلاغات السابقة محفوظة.", "No se pudo guardar; los informes anteriores se conservaron."},
    {"SAVED LOCALLY; SENDING IF ONLINE.", "تم الحفظ محلياً؛ سيُرسل عند توفر الخدمة.", "Guardado localmente; se enviará cuando el servicio esté disponible."},
    {"ENTER TITLE + DETAILS.", "أدخل العنوان والتفاصيل.", "Introduce título y detalles."},
    {"REPORT TITLE", "عنوان البلاغ", "TÍTULO DEL INFORME"},
    {"REPORT DETAILS", "تفاصيل البلاغ", "DETALLES DEL INFORME"},
    {"Enter text for your support report", "أدخل نص بلاغ الدعم", "Escribe el texto del informe de soporte"},
    {"Guide & Support", "الدليل والدعم", "Guía y soporte"},
    {"PS2 ENHANCER", "محسّن ألعاب PS2", "Mejoras de PS2"},
    {"Per-game netemu CONFIG tools; ISO remains untouched.", "أدوات إعداد لكل لعبة؛ ملف ISO الأصلي لا يتغير.", "Herramientas CONFIG por juego; el ISO permanece intacto."},
    {"L2H PERFORMANCE (0x46)", "تحسين الأداء L2H (0x46)", "Rendimiento L2H (0x46)"},
    {"May help titles using L2H downloads.", "قد يساعد الألعاب التي تستخدم تنزيلات L2H.", "Puede ayudar a juegos que usan descargas L2H."},
    {"SHARPER PIXELS (0x44)", "بكسلات أوضح (0x44)", "Píxeles más nítidos (0x44)"},
    {"Disables smoothing; aliasing may increase.", "يعطّل التنعيم وقد يزيد التعرّج.", "Desactiva el suavizado; puede aumentar el aliasing."},
    {"FRAME PACING (0x48)", "توقيت الإطارات (0x48)", "Ritmo de fotogramas (0x48)"},
    {"PRESSURE SENSITIVITY (0x50)", "حساسية الضغط (0x50)", "Sensibilidad a la presión (0x50)"},
    {"Enables analog pressure-sensitive button input.", "يفعّل استجابة الأزرار التناظرية للضغط.", "Activa la respuesta analógica sensible a la presión."},
    {"PRESSURE SENSITIVITY ENABLED.", "تم تفعيل حساسية الضغط.", "Sensibilidad a la presión activada."},
    {"PRESSURE SENSITIVITY DISABLED.", "تم تعطيل حساسية الضغط.", "Sensibilidad a la presión desactivada."},
    {"Conservative VSYNC timing; experimental.", "توقيت VSYNC محافظ؛ خيار تجريبي.", "Temporización VSYNC conservadora; experimental."},
    {"APPLY LOCAL PROFILE", "تطبيق الملف المحلي", "Aplicar perfil local"},
    {"DOWNLOAD EXACT GAME CONFIG", "تنزيل إعداد اللعبة المطابق", "Descargar CONFIG exacta"},
    {"BACK UP CURRENT CONFIG", "نسخ الإعداد الحالي احتياطياً", "Crear copia del CONFIG actual"},
    {"RESTORE SAVED BACKUP", "استعادة النسخة الاحتياطية", "Restaurar copia guardada"},
    {"ISO PROFILE ONLY", "ملف ISO فقط", "Solo perfil ISO"},
    {"NO EXACT GAME CONFIG", "لا يوجد إعداد مطابق للعبة", "No hay CONFIG exacta"},
    {"XMB upscaler / smoothing are separate display settings.", "خيارات رفع الدقة والتنعيم في XMB مستقلة عن هذه الإعدادات.", "El escalado y suavizado XMB son ajustes de pantalla independientes."},
    {"60 FPS requires an exact per-game code patch; these options do not unlock it.", "يتطلب 60 FPS تعديلاً برمجياً مطابقاً للعبة؛ هذه الخيارات لا تفتح 60 إطاراً.", "60 FPS requiere un parche de código exacto por juego; estas opciones no lo desbloquean."},
    {"UP/DOWN: SELECT | CROSS: TOGGLE / RUN | CIRCLE / SELECT: BACK", "أعلى/أسفل: اختيار | X: تبديل/تنفيذ | دائرة/SELECT: رجوع", "Arriba/abajo: elegir | X: cambiar/ejecutar | Círculo/SELECT: volver"},
    {"L2H IS ON BY DEFAULT. SELECT OPTIONS THEN APPLY.", "تحسين L2H مفعّل افتراضياً. اختر الخيارات ثم طبّقها.", "L2H está activado por defecto. Elige opciones y aplica."},
    {"EXISTING COMMUNITY CONFIG DETECTED. LOCAL PROFILE WILL NOT OVERWRITE ITS FIXES.", "تم العثور على إعداد مجتمع موجود. لن يستبدل الملف المحلي إصلاحاته.", "Se detectó un CONFIG comunitario. El perfil local no sobrescribirá sus ajustes."},
    {"EXISTING CONFIG DETECTED. LOCAL PROFILE WILL NOT OVERWRITE IT.", "تم العثور على إعداد موجود. لن يستبدله الملف المحلي.", "Se detectó un CONFIG existente. El perfil local no lo sobrescribirá."},
    {"RESTORE BACKUP FIRST", "استعد النسخة الاحتياطية أولاً", "Restaura primero la copia de seguridad"},
    {"APPLY EXPERIMENTAL 60 FPS PATCH", "تطبيق تعديل 60 إطاراً تجريبياً", "Aplicar parche experimental de 60 FPS"},
    {"ISO PATCH ONLY", "تعديل ISO فقط", "Solo parche ISO"},
    {"FPS patch needs exact serial + ELF hash.", "تعديل FPS يتطلب تطابق الرقم التسلسلي وبصمة ELF.", "El parche FPS requiere coincidencia exacta de serie y hash ELF."},
    {"60 FPS patch is experimental; it is not verified on hardware.", "تعديل 60 FPS تجريبي ولم يُختبر على الجهاز فعلياً.", "El parche de 60 FPS es experimental y no está verificado en hardware."},
    {"FPS PATCHES ARE AVAILABLE FOR PS2 ISO FILES ONLY.", "تعديلات FPS متاحة لملفات PS2 ISO فقط.", "Los parches FPS solo están disponibles para archivos ISO de PS2."},
    {"Game patches require exact serial + ELF hash.", "تعديلات الألعاب تتطلب تطابق الرقم التسلسلي وبصمة ELF.", "Los parches requieren coincidencia exacta de serie y hash ELF."},
    {"FPS / widescreen patches are experimental; hardware testing is pending.", "تعديلات FPS والشاشة العريضة تجريبية ولم تُختبر على الجهاز بعد.", "Los parches FPS/panorámicos son experimentales; falta probarlos en hardware."},
    {"NO EXACT 16:9 PATCH MATCH FOR THIS GAME SERIAL / ELF HASH.", "لا يوجد تعديل 16:9 يطابق الرقم التسلسلي وبصمة ELF لهذه اللعبة.", "No hay parche 16:9 para esta serie y hash ELF exactos."},
    {"EXPERIMENTAL 16:9 PATCH INSTALLED. ORIGINAL ISO WAS NOT CHANGED.", "تم تثبيت تعديل 16:9 التجريبي دون تغيير ملف ISO الأصلي.", "Parche 16:9 experimental instalado. El ISO original no se modificó."},
    {"GAME PATCHES ARE AVAILABLE FOR PS2 ISO FILES ONLY.", "تعديلات الألعاب متاحة لملفات PS2 ISO فقط.", "Los parches solo están disponibles para ISO de PS2."},
    {"GAME PATCH CONFIG BUILD FAILED. NO FILE WAS CHANGED.", "فشل إنشاء إعداد تعديل اللعبة. لم يتغير أي ملف.", "Falló la creación del CONFIG del parche. No se modificó ningún archivo."},
    {"COULD NOT STAGE GAME PATCH CONFIG. CHECK TEMP FILES.", "تعذر تجهيز إعداد تعديل اللعبة. افحص الملفات المؤقتة.", "No se pudo preparar el CONFIG del parche. Revisa los temporales."},
    {"GAME PATCH INSTALL FAILED. ORIGINAL CONFIG WAS PRESERVED.", "فشل تثبيت تعديل اللعبة. تم الحفاظ على الإعداد الأصلي.", "Falló la instalación del parche. Se conservó el CONFIG original."},
    {"PS2 BOOT ELF COULD NOT BE READ OR HASHED.", "تعذرت قراءة ملف إقلاع PS2 أو حساب بصمته.", "No se pudo leer ni calcular el hash del ELF de arranque PS2."},
    {"NO EXACT 60 FPS PATCH MATCH FOR THIS GAME SERIAL / ELF HASH.", "لا يوجد تعديل 60 FPS يطابق الرقم التسلسلي وبصمة ELF لهذه اللعبة.", "No hay un parche de 60 FPS para esta serie y hash ELF exactos."},
    {"PATCH ADDRESS IS NOT IN A FILE-BACKED EXECUTABLE ELF SEGMENT.", "عنوان التعديل ليس ضمن مقطع تنفيذي صالح في ELF.", "La dirección del parche no pertenece a un segmento ejecutable válido del ELF."},
    {"EXISTING CONFIG FORMAT CANNOT BE MERGED SAFELY.", "تنسيق الإعداد الحالي لا يسمح بالدمج الآمن.", "El formato CONFIG actual no permite una combinación segura."},
    {"TITLE ID / MULTIDISC CONFIG CANNOT BE REORDERED SAFELY.", "لا يمكن إعادة ترتيب إعداد Title ID أو تعدد الأقراص بأمان.", "No se puede reordenar de forma segura el CONFIG de Title ID o multidisco."},
    {"CONFIG ALREADY CONTAINS AN EE CODE PATCH. NO DUPLICATE ADDED.", "الإعداد يحتوي مسبقاً على تعديل EE؛ لم تتم إضافة نسخة مكررة.", "El CONFIG ya contiene un parche EE; no se añadió un duplicado."},
    {"CONFIG CANNOT BE MERGED SAFELY. NO FILE WAS CHANGED.", "تعذر دمج الإعداد بأمان. لم يتغير أي ملف.", "No se pudo combinar el CONFIG de forma segura. No se modificó ningún archivo."},
    {"FPS PATCH CONFIG BUILD FAILED. NO FILE WAS CHANGED.", "فشل إنشاء إعداد تعديل FPS. لم يتغير أي ملف.", "Falló la creación del CONFIG del parche FPS. No se modificó ningún archivo."},
    {"COULD NOT STAGE FPS PATCH CONFIG. CHECK TEMP FILES.", "تعذر تجهيز إعداد تعديل FPS. افحص الملفات المؤقتة.", "No se pudo preparar el CONFIG FPS. Revisa los archivos temporales."},
    {"EXPERIMENTAL 60 FPS PATCH INSTALLED. EXISTING CONFIG PRESERVED.", "تم تثبيت تعديل 60 FPS التجريبي مع الحفاظ على الإعداد الموجود.", "Parche experimental de 60 FPS instalado. Se conservó el CONFIG existente."},
    {"EXPERIMENTAL 60 FPS PATCH INSTALLED. NEW CONFIG CREATED.", "تم تثبيت تعديل 60 FPS التجريبي وإنشاء إعداد جديد.", "Parche experimental de 60 FPS instalado. Se creó un CONFIG nuevo."},
    {"FPS PATCH INSTALL FAILED. ORIGINAL CONFIG WAS PRESERVED.", "فشل تثبيت تعديل FPS. تم الحفاظ على الإعداد الأصلي.", "Falló la instalación del parche FPS. Se conservó el CONFIG original."},
    {"PS3 XMB IMAGE SETTINGS", "إعدادات الصورة في XMB على PS3", "Ajustes de imagen XMB de PS3"},
    {"Settings > Game Settings > PS / PS2 Upscaler", "الإعدادات > إعدادات اللعبة > تحسين PS / PS2", "Ajustes > Ajustes de juego > Escalador PS / PS2"},
    {"Normal: upscale while preserving the original aspect ratio.", "Normal: تحسين الإخراج مع الحفاظ على نسبة العرض الأصلية.", "Normal: ampliar sin cambiar la relación de aspecto original."},
    {"Full Screen: stretch to fill the screen; aspect ratio changes.", "Full Screen: تمديد الصورة لملء الشاشة مع تغيير نسبة العرض.", "Pantalla completa: estirar para llenar la pantalla; cambia la proporción."},
    {"PS / PS2 Smoothing", "تنعيم PS / PS2", "Suavizado de PS / PS2"},
    {"On: soften jagged edges; may look blurrier.", "On: تنعيم الحواف المتعرجة وقد يجعل الصورة أكثر ضبابية.", "Activado: suaviza bordes dentados; puede verse más borroso."},
    {"Off: keep pixels sharper; aliasing may be more visible.", "Off: إبقاء البكسلات أوضح مع احتمال ظهور الحواف المتعرجة.", "Desactivado: conserva píxeles más nítidos; puede notarse más el aliasing."},
    {"Display Settings > Video Output Settings: choose a supported 720p / 1080p output.", "إعدادات العرض > إعدادات إخراج الفيديو: اختر 720p أو 1080p مدعوماً.", "Ajustes de pantalla > Salida de vídeo: elige una salida compatible de 720p / 1080p."},
    {"This upscales the output; it does not render the PS2 game natively at HD.", "هذا يحسن دقة الإخراج ولا يجعل اللعبة تعمل بدقة HD أصلية.", "Esto amplía la salida; no renderiza el juego de PS2 de forma nativa en HD."},
    {"These are global XMB settings. PS3 SCALE cannot change them automatically.", "هذه إعدادات عامة في XMB ولا يستطيع PS3 SCALE تغييرها تلقائياً.", "Son ajustes globales de XMB. PS3 SCALE no puede cambiarlos automáticamente."},
    {"TRIANGLE / CIRCLE: CLOSE DISPLAY GUIDE", "مثلث / دائرة: إغلاق دليل العرض", "TRIÁNGULO / CÍRCULO: CERRAR GUÍA"},
    {"UP/DOWN: SELECT | CROSS: TOGGLE / RUN | TRIANGLE: DISPLAY GUIDE | CIRCLE: BACK", "أعلى/أسفل: اختيار | X: تبديل/تشغيل | مثلث: دليل العرض | دائرة: رجوع", "ARRIBA/ABAJO: ELEGIR | X: CAMBIAR/EJECUTAR | TRIÁNGULO: GUÍA | CÍRCULO: ATRÁS"},
    {"SAVED BACKUP DOES NOT MATCH. RESTORE IT BEFORE CHANGING PROFILE.", "النسخة الاحتياطية لا تطابق الإعداد الحالي. استعدها قبل تغيير الملف.", "La copia no coincide. Restáurala antes de cambiar el perfil."},
    {"AUTOMATIC BACKUP FAILED. NO FILE WAS CHANGED.", "فشل النسخ الاحتياطي التلقائي. لم يتغير أي ملف.", "Falló la copia automática. No se cambió ningún archivo."},
    {"PS2 ENHANCEMENT CONFIG INSTALLED. NO PREVIOUS CONFIG EXISTED.", "تم تثبيت إعداد تحسين PS2. لم يكن هناك إعداد سابق.", "CONFIG de mejora PS2 instalado. No existía un CONFIG anterior."},
    {"PS2 CONFIG INSTALLED. GAME ISO AND EBOOT WERE NOT MODIFIED.", "تم تثبيت إعداد PS2. لم يتم تعديل ISO أو EBOOT اللعبة.", "CONFIG de PS2 instalado. El ISO y EBOOT del juego no se modificaron."},
    {"SAVED BACKUP DOES NOT MATCH. RESTORE IT BEFORE REPLACING CONFIG.", "النسخة الاحتياطية لا تطابق الإعداد الحالي. استعدها قبل الاستبدال.", "La copia no coincide. Restáurala antes de reemplazar el CONFIG."},
    {"L2H PERFORMANCE ENABLED.", "تم تفعيل تحسين L2H.", "Rendimiento L2H activado."},
    {"L2H PERFORMANCE DISABLED.", "تم تعطيل تحسين L2H.", "Rendimiento L2H desactivado."},
    {"SHARPER PIXELS ENABLED.", "تم تفعيل وضوح البكسلات.", "Píxeles más nítidos activados."},
    {"SHARPER PIXELS DISABLED.", "تم تعطيل وضوح البكسلات.", "Píxeles más nítidos desactivados."},
    {"FRAME PACING ENABLED (EXPERIMENTAL).", "تم تفعيل توقيت الإطارات (تجريبي).", "Ritmo de fotogramas activado (experimental)."},
    {"FRAME PACING DISABLED.", "تم تعطيل توقيت الإطارات.", "Ritmo de fotogramas desactivado."},
    {"LOCAL PROFILES REQUIRE A PS2 ISO; CLASSICS NEED AN EXACT ENC CONFIG.", "الملفات المحلية تتطلب PS2 ISO؛ ألعاب Classics تحتاج ENC مطابقاً.", "Los perfiles locales requieren un ISO de PS2; Classics necesitan un ENC exacto."},
    {"SELECT AT LEAST ONE ENHANCEMENT OPTION.", "اختر خيار تحسين واحداً على الأقل.", "Selecciona al menos una opción de mejora."},
    {"BACKUP FAILED OR DOES NOT MATCH CURRENT CONFIG. NO FILE WAS CHANGED.", "فشل النسخ الاحتياطي أو لا يطابق الإعداد الحالي. لم يتغير أي ملف.", "La copia falló o no coincide con el CONFIG actual. No se cambió ningún archivo."},
    {"CONFIG BUILD FAILED. NO FILE WAS CHANGED.", "فشل إنشاء الإعداد. لم يتغير أي ملف.", "Falló la creación del CONFIG. No se cambió ningún archivo."},
    {"COULD NOT STAGE CONFIG. CHECK TEMP FILES AND FREE SPACE.", "تعذر تجهيز الإعداد. افحص الملفات المؤقتة والمساحة.", "No se pudo preparar el CONFIG. Revisa temporales y espacio."},
    {"PS2 ENHANCEMENT CONFIG INSTALLED. ORIGINAL CONFIG BACKED UP.", "تم تثبيت إعداد تحسين PS2 وحفظ نسخة من الإعداد الأصلي.", "CONFIG de mejora PS2 instalado. CONFIG original respaldado."},
    {"BACKUP CHANGED. NO CONFIG WAS REPLACED.", "تغيّرت النسخة الاحتياطية. لم يتم استبدال الإعداد.", "La copia cambió. No se reemplazó el CONFIG."},
    {"ROLLBACK FILE EXISTS OR IS INVALID. NO FILE WAS OVERWRITTEN.", "ملف التراجع موجود أو غير صالح. لم تتم الكتابة فوق أي ملف.", "El archivo de reversión existe o no es válido. No se sobrescribió nada."},
    {"CONFIG INSTALL FAILED. ORIGINAL FILE WAS PRESERVED.", "فشل تثبيت الإعداد. تم الحفاظ على الملف الأصلي.", "Falló la instalación del CONFIG. Se conservó el archivo original."},
    {"AUTOMATIC BACKUP FAILED OR DOES NOT MATCH. NO REPLACEMENT WAS MADE.", "فشل النسخ الاحتياطي التلقائي أو لا يطابق الملف. لم يتم الاستبدال.", "La copia automática falló o no coincide. No se reemplazó nada."},
    {"PS2 CONFIG / ENHANCEMENT AVAILABLE", "إعداد/تحسين PS2 متاح", "CONFIG / mejora de PS2 disponible"},
    {"Game-specific timing and compatibility fixes.", "إصلاحات توقيت وتوافق خاصة بكل لعبة.", "Ajustes de tiempo y compatibilidad por juego."},
    {"No universal 60 FPS or HD texture unlock.", "لا يوجد فتح عام لـ60 FPS أو خامات HD.", "No existe desbloqueo universal de 60 FPS ni texturas HD."},
    {"XMB upscaler is a separate display setting.", "رفع الدقة في XMB إعداد عرض منفصل.", "El escalado XMB es un ajuste de pantalla independiente."},
    {"SQUARE: PS2 ENHANCER", "مربع: محسّن ألعاب PS2", "Cuadrado: mejoras de PS2"}
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
    {"UPDATE PKG FOUND ON USB", "تم العثور على حزمة تحديث على USB", "Paquete de actualización encontrado en USB"},
    {"ONLINE MATCHES: ", "مطابقات أونلاين: ", "Coincidencias en línea: "},
    {"TITLE ID: ", "معرّف اللعبة: ", "ID del juego: "},
    {"APP VERSION: ", "إصدار التطبيق: ", "Versión de la aplicación: "},
    {"ONLINE PROFILE: ", "ملف أونلاين: ", "Perfil en línea: "},
    {"EFFECT: ", "التأثير: ", "Efecto: "},
    {"AUTO APPLY: ", "التطبيق التلقائي: ", "Aplicación automática: "},
    {"MEMORY WRITES: ", "عمليات تعديل الذاكرة: ", "Escrituras de memoria: "},
    {"DATABASE: ", "قاعدة البيانات: ", "Base de datos: "},
    {"Game: ", "اللعبة: ", "Juego: "},
    {"CONFIG file: ", "ملف CONFIG: ", "Archivo CONFIG: "},
    {"Saved backup: ", "النسخة الاحتياطية: ", "Copia guardada: "},
    {"Rollback: ", "التراجع: ", "Reversión: "},
    {"Match: ", "المطابقة: ", "Coincidencia: "},
    {"ID: ", "المعرّف: ", "ID: "},
    {"CONFIG size: ", "حجم CONFIG: ", "Tamaño CONFIG: "},
    {"Target: ", "الهدف: ", "Destino: "}
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

    if (strncmp(source, "DATABASE: ", 10) == 0) {
        char translated_status[128];
        (void)ps3scale_locale_translate(language, source + 10,
                                        translated_status,
                                        sizeof(translated_status));
        if (language == PS3SCALE_LANG_ARABIC)
            snprintf(output, output_size, "قاعدة البيانات: %s", translated_status);
        else
            snprintf(output, output_size, "Base de datos: %s", translated_status);
        return 1;
    }

    {
        unsigned profiles, matches, cached;
        if (sscanf(source, "PROFILES: %u  |  EXACT LIBRARY MATCHES: %u",
                   &profiles, &matches) == 2) {
            if (language == PS3SCALE_LANG_ARABIC)
                snprintf(output, output_size,
                         "الملفات: %u | مطابقات المكتبة الدقيقة: %u",
                         profiles, matches);
            else
                snprintf(output, output_size,
                         "Perfiles: %u | coincidencias exactas: %u",
                         profiles, matches);
            return 1;
        }
        if (sscanf(source,
                   "CACHED %u MATCHED PROFILE RECORDS. GAME FILES WERE NOT CHANGED.",
                   &cached) == 1) {
            if (language == PS3SCALE_LANG_ARABIC)
                snprintf(output, output_size,
                         "تم حفظ %u ملفات مطابقة. لم تتغير ملفات الألعاب.",
                         cached);
            else
                snprintf(output, output_size,
                         "Guardados %u perfiles. No se cambiaron juegos.",
                         cached);
            return 1;
        }
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
    return glyph >= 128u ? 1 : 0;
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
