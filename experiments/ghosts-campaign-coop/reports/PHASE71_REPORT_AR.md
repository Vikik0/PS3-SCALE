# تقرير المرحلة 71 — توسيع بحث Ghidra حسب أدلة Client 2

## الهدف

بعد اكتشاف playerpiggyback وplayengineeffects وربط FOV للعنصرين 0 و1، وسعت قائمة الكلمات التي يبحث عنها Ghidra داخل ملفات PPU.

## التعديلات

1. أضفت playerpiggyback وplayengineeffects إلى قائمة البحث.
2. أضفت cg_playerFovScale0 وcg_playerFovScale1.
3. أضفت hideonclient وshowonclient وcreateclientfontstring.
4. أبقيت مصطلحات split-screen السابقة مثل PlayerCMD_Spawn وSV_InitLocalClient وCL_InitLocalClients.
5. تستمر أداة المقارنة في اعتبار مراجع CALL فقط أدلة لمسار الاستدعاء، وتفصل عنها مراجع DATA وComputed.

## التحقق

تم توسيع اختبار المصدر ليتأكد من وجود جميع الكلمات الجديدة في Ghidra exporter. كما تم تصحيح اختبار Phase 70 الذي كان يتوقع ملفين بينما fixture يحتوي ملف GSC متداخلاً واحداً فقط.

## الحدود

هذه تهيئة بحث فقط. لم يتم تشغيل Ghidra على PPU فعلي لعدم توفر ملفات ELF المفكوكة المطابقة لـTU 1.16. عدم ظهور نتيجة لا يثبت غياب الوظيفة.
