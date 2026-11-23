# المرحلة العاشرة — فحص حزمة Runtime دفعة واحدة

## الهدف

بعد جمع ملفات Campaign وMultiplayer من جهاز PS3، كان يلزم تشغيل عدة أدوات منفصلة والتحقق يدوياً من أن كل تقرير يعود إلى الوضع الصحيح. أضيف الآن مشغّل واحد يجمع التحقق وإعادة البناء والمقارنة في تقرير موحد.

الأداة لا تتصل بالـPS3 ولا تحتاج إنترنت. تعمل محلياً على ملفات المستخدم الموجودة على الكمبيوتر.

## المدخلات

يجب توفير:

- default.self من تحديث BLUS31270 رقم 1.16 الخاص بـCampaign.
- default_mp.self من التحديث نفسه الخاص بـMultiplayer.
- مجلد Campaign الذي يحتوي ملفات GHOSTS_TU116_CAMPAIGN_SEG0.bin وSEG1.bin.
- مجلد Multiplayer الذي يحتوي ملفات GHOSTS_TU116_MULTIPLAYER_SEG0.bin وSEG1.bin.
- سجلا Pad Probe اختياريان، لكن يجب تقديمهما معاً أو تركهما معاً.

لا تخلط ملفات القرص 1.00 مع تحديث 1.16، ولا تخلط مقاطع Campaign مع Multiplayer.

## التشغيل على Windows

من مجلد experiments/ghosts-campaign-coop شغّل:

~~~powershell
python tools/ps3mapi/analyze_tu116_runtime_capture.py --campaign-self "D:\Ghosts\TU116\default.self" --campaign-dumps "D:\Ghosts\captures\campaign" --multiplayer-self "D:\Ghosts\TU116\default_mp.self" --multiplayer-dumps "D:\Ghosts\captures\multiplayer" --campaign-log "D:\Ghosts\captures\ghosts_campaign_pad_probe.log" --multiplayer-log "D:\Ghosts\captures\ghosts_multiplayer_pad_probe.log" --output-dir "D:\Ghosts\analysis\run-01"
~~~

إذا لم تتوفر سجلات Pad Probe بعد، احذف الخيارين --campaign-log و--multiplayer-log معاً. سيكمل فحص ELF ويذكر أن دليل المنافذ غير متوفر.

إذا كان مجلد النتائج يحتوي ملفات من تشغيل سابق، تتوقف الأداة ولا تستبدلها. استخدم --overwrite فقط عندما تقصد استبدال تقارير التحليل السابقة.

تُنشئ الأداة النتائج أولاً داخل مجلد مرحلي مؤقت، ولا تنقلها إلى مجلد النتائج النهائي إلا بعد نجاح جميع خطوات التحقق والمقارنة. إذا فشل فحص Campaign أو Multiplayer أو أي خطوة لاحقة، تُحذف النتائج المرحلية وتبقى الملفات النهائية السابقة كما هي. عند استخدام --overwrite، تستبدل الأداة ملفات التحليل العشرة المحددة فقط، وتحافظ على الملفات الأخرى داخل مجلد النتائج. وإذا تعذر إكمال نقل الملفات، تحاول استعادة النسخ السابقة قبل إظهار الخطأ.

## ما الذي تنفذه الأداة؟

1. تتحقق من وجود الملفات والمجلدات ومن فصل مدخلات Campaign عن Multiplayer.
2. تقرأ رؤوس SELF وتقارن نقطة الدخول وPT_LOAD مع خريطة TU 1.16 المحددة.
3. تتحقق من أحجام مقاطع PS3MAPI، ومن عدم كون المقطع التنفيذي فارغاً أو صفرياً.
4. تعيد بناء ELF64 Big-Endian PowerPC64 تحليلي لكل وضع.
5. تفحص النصوص المرتبطة بالـLocal Client وSplit-screen وController وCamera وHUD.
6. تقارن النصوص ومقاطع PT_LOAD المتداخلة بين الوضعين.
7. تقارن سجلات Pad Probe، وتتحقق أولاً من اكتمالها ومصدرها، ثم تتحقق من ظهور port 0 وport 1 معاً في العينة نفسها.
8. تكتب تقريراً موحداً وبصمات SHA-256 للمدخلات الأساسية.

## شروط قبول سجلات Pad Probe — Phase 25

لا يكفي وجود عينة واحدة تحمل `port_mask=0x03`. يجب أن يحتوي كل سجل على:

- ترويسة Pad Probe v4 وعلامة `READ_ONLY=1`.
- وضع صحيح محدد يدوياً من ملف الوضع، مع `MODE_SOURCE=manual_mode_file` و`MODE_LABEL_IS_OPERATOR_SELECTED=1`.
- عينات ناجحة عند المؤشرات 0 و5 و10 و15 و20 و25.
- علامة النهاية `Pad probe finished.`.
- عدم وجود أرقام عينات مكررة أو خارج المجال 0–29.

إذا فشل أي شرط، يبقى التقرير في حالة `ELF_ANALYSIS_READY_PAD_GATE_NOT_PASSED` ولا ينتقل إلى مراجعة المحرك. يعرض `capture_summary.json` اكتمال السجل وصحة مصدره كحقلين منفصلين.

## الملفات الناتجة

داخل مجلد النتائج:

- campaign_tu116_runtime.elf وmultiplayer_tu116_runtime.elf.
- تقريران للتحقق من SELF ولقطات الذاكرة.
- تقريران للنصوص المستخرجة.
- مقارنة النصوص ومقارنة كتل ELF.
- pad_probe_comparison.json.
- capture_summary.json وهو ملخص النتيجة وبوابة العمل التالية.

## تفسير الحالات

- CAPTURE_READY_FOR_ENGINE_REVIEW: ملفات الوضعين اجتازت التحقق البنيوي، وسجلات Pad Probe أظهرت المنفذين 0 و1 معاً في الوضعين. هذا يسمح بمراجعة مسار المحرك، ولا يثبت إنشاء اللاعب الثاني. يشترط هذا التصنيف أيضاً اجتياز سجلي v4 الكاملين وفحص مصدر وضع التشغيل كما هو موضح في Phase 25.
- ELF_ANALYSIS_READY_PAD_EVIDENCE_MISSING: تمت إعادة بناء ELF، لكن سجلات Pad Probe لم تُقدّم.
- ELF_ANALYSIS_READY_PAD_GATE_NOT_PASSED: تمت إعادة بناء ELF، لكن دليل ظهور المنفذين معاً لم يجتز الشرط.
- CAPTURE_VERIFICATION_FAILED: فشل التحقق من ملف أو مقطع؛ لا تعتمد على النتائج.

في جميع الحالات يبقى player_two_gate.status هو NOT_PROVEN حتى يثبت تحليل runtime وجود Local Client ثانٍ مستقل وربط Controller 2 به ووجود Player State وكاميرا وHUD منفصلة.

## حدود السلامة والدقة

- PASS_METADATA يعني تطابق بنية SELF ومقاطع PT_LOAD مع خريطة TU 1.16 المسجلة؛ وليس إثباتاً تشفيرياً لهوية التحديث.
- اسم الوضع في السجل يختاره المشغّل من ملف mode؛ لا يوجد اكتشاف تلقائي لـCampaign أو Multiplayer.
- ظهور port_mask=0x03 يثبت حالة طبقة Pad فقط.
- اختلاف كتلة بايتات لا يحدد بداية دالة أو عنوان Hook.
- ELF الناتج مخصص للتحليل الثابت فقط، وليس EBOOT قابلاً للتشغيل.
- لا تعدّل الأداة ملفات اللعبة، ولا تفك تشفير SELF، ولا تعيد بناء FastFiles موقعة، ولا تنشئ PKG أو لاعباً ثانياً.
- احتفظ بملفات SELF ولقطات الذاكرة الخام على جهازك. شارك تقارير JSON والبصمات فقط ما لم نحدد لاحقاً حاجة فعلية إلى نطاق بايتات معين.
