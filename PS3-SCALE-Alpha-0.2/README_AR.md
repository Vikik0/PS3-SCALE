# PS3 SCALE — Alpha 0.2 (Source Build)

**هدف هذه الحزمة:** أول أساس برمجي حقيقي للمشروع: قراءة معلومات الألعاب وبناء مكتبة آمنة للـ PS3 وPS2 على جهاز الاختبار CECH-2504A / CFW 4.93.

## ما الموجود فعلياً في هذه النسخة؟
- محلّل PARAM.SFO مكتوب بلغة C مع فحوص حدود الذاكرة.
- ماسح مجلدات PS3 في `GAMES` و`GAMEZ`.
- قراءة الألعاب المثبتة من `/dev_hdd0/game/` حسب CATEGORY:
  - `HG`: لعبة PS3 مثبتة على HDD.
  - `2P`: PS2 Classics.
  - `2G`: تثبيت PS2 من قرص على الأجهزة/الأنظمة التي تدعمه.
- اكتشاف ملفات `.iso` في `PS3ISO` و`PS2ISO`.
- استخراج PS2 serial من ISO9660 عبر `SYSTEM.CNF` إن كان موجوداً.
- محاولة استخراج PS3 Title ID من أسماء ISO الشائعة.
- قراءة القرص المركّب مباشرة من `/dev_bdvd/PS3_GAME/PARAM.SFO`.
- دعم مجلدات التخزين المباشرة والمجلدات داخل `PS3/`.
- مسح `/dev_hdd0` و`/dev_bdvd` و`/dev_usb000` إلى `/dev_usb007`.
- فحص Read-Only: لا يوجد كود يكتب أو يحذف أو يعيد تسمية ملفات الألعاب.
- واجهة أولية تعرض المكتبة ومعلومات اللعبة.

## ما الذي لم يُنفّذ بعد؟
هذه ليست PKG جاهزة للتثبيت بعد، ولم يتم اختبارها على جهاز PS3 فعلي. بيئة التنفيذ الحالية لا تحتوي على PPU cross-compiler ولا أدوات توقيع/تغليف PS3؛ لذلك لن أقدّم ملف PKG وهمياً أو أقول إنه اختُبر.

كذلك محرك تحسين الرسومات أثناء اللعب غير موجود في Alpha 0.2. سنبنيه بعد إثبات مسار تقني قابل للتطبيق على CFW، غالباً كتعديلات خاصة بكل لعبة أو PRX per-title. تطبيق مدير عادي لا يستطيع وحده فرض فلتر على صورة كل لعبة بعد إغلاقه.

## جهاز الاختبار المعتمد
- Model: CECH-2504A (Slim)
- Firmware: 4.93 (الصور المرسلة تؤكد 4.93، وليس 4.92)
- CFW: CEX
- Cobra: 8.5
- مساحة فارغة ظاهرة: قرابة 10 GB

لا نحتاج IDPS أو PSID أو MAC أو الرقم التسلسلي لتطوير التطبيق. لا تشارك هذه المعرفات علناً.

## اختبار من الكمبيوتر (لا يحتاج PS3)
```sh
chmod +x scripts/test_host.sh
./scripts/test_host.sh
```
الاختبار يفحص محلّل PARAM.SFO على الكمبيوتر فقط.

## بناء PKG بدون تثبيت أدوات على جهازك (GitHub Actions)
أضفت Workflow في `.github/workflows/build-ps3.yml`:
- يختبر محلّل SFO والماسح وقراءة ISO على Linux.
- يستخدم صورة `ghcr.io/02900/ps3-toolchain-tiny3d` التي تجمع PS3 toolchain وPSL1GHT وTiny3D.
- يبني ELF وSELF وPKG ثم يرفعها كـ Artifact إذا نجح البناء.

لم يتم تشغيل Workflow على مستودعك بعد لأن المستودع ما زال فارغاً ولا أملك صلاحية Push إليه. لن أصف ملف PKG بأنه جاهز قبل نجاح البناء ثم اختباره على جهازك.

## بناء PKG محلياً عند توفر بيئة PS3DEV
المشروع يعتمد على:
- PS3DEV / PSL1GHT
- Tiny3D + libfont (لواجهة RSX)
- أدوات SELF/PKG التي يوفرها PSL1GHT

راجع `docs/RESEARCH_NOTES.md` لملخص أبحاث المطورين ومصادر multiMAN/webMAN MOD/IRISMAN وPS2 CONFIG وFPS patches.

بعد إعداد البيئة:
```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make pkg
```

## مسارات البحث
- PS3 folder games: `/dev_hdd0/GAMES`, `/dev_hdd0/GAMEZ`
- PS3 installed titles: `/dev_hdd0/game`
- PS3 ISO: `/dev_hdd0/PS3ISO`
- PS2 ISO: `/dev_hdd0/PS2ISO`
- USB: `/dev_usb000` إلى `/dev_usb007`

ملاحظة: PS2 ISO يحاول استخراج رقم اللعبة من `SYSTEM.CNF`، أما PS3 ISO فيُستخرج Title ID من اسم الملف فقط إذا كان مكتوباً فيه. قراءة بيانات PS3 ISO الداخلية واستخراج ICON0/PIC1 مرحلة لاحقة.

## التحكم في Alpha
- D-Pad Up/Down: اختيار لعبة.
- Square: إعادة الفحص.
- Cross: عرض التفاصيل (تظهر في اللوحة اليمنى).
- Circle: خروج.

## حماية البيانات
Alpha 0.2 لا تطبّق patches ولا تغيّر أي ملف لعبة. لا تفعّل أي تعديل تلقائي في النسخ القادمة قبل:
1. مطابقة Title ID والمنطقة وإصدار التحديث.
2. التحقق من SHA-256.
3. أخذ نسخة احتياطية.
4. توفير Rollback.
5. اختبار الملف على الجهاز المستهدف.
