# تقرير المرحلة 78 — تجهيز حزمة التقاط TU 1.16

## الناتج

تم تجهيز حزمة مستقلة باسم `Ghosts_TU116_Capture_Kit_Phase77.zip` لتسهيل تنفيذ التقاط Campaign وMultiplayer على جهاز PS3. تحتوي على:

- سكربتي PS3MAPI اليدويين لالتقاط Campaign وMultiplayer.
- Pad Probe v4 بصيغة `.fake.sprx`.
- ملف وضع منفصل لكل من Campaign وMultiplayer.
- تعليمات عربية موحدة لخطوات التحميل والالتقاط ونقل النتائج.

لم تتم إضافة ملفات اللعبة الأصلية أو SELF أو EBOOT إلى الحزمة.

## إثبات مصدر Pad Probe

- GitHub Actions run: `45482944832`.
- مصدر البناء: commit `c27b740985791c818154ce50954b5457716d26a3`.
- Artifact ID: `14963316034`.
- SHA-256 للـZIP الأصلي: `9a6d54e14cdc83aa7736d504d88cf902775962cbfa0b27ab46e63666998bd17d`، مطابق لبصمة GitHub Actions.
- SHA-256 لملف `ghosts_campaign_pad_probe.fake.sprx`: `4de0ea6f49629818e4fd69d4f3e44e9357fff4f7481ad87ba07888e9542b574a`.
- تمت مقارنة `module.c` و`Makefile` و`exports.S` وملفي الوضع بين commit البناء والفرع الحالي؛ جميعها دون تغيير.

## إثبات مصدر سكربتات الذاكرة

تم أخذ سكربتي Campaign وMultiplayer من الفرع الحالي، والتحقق من Git blob SHA لكل ملف مقابل قيمته في GitHub:

- Campaign: `ddfe4e943c78babd8ab75a164b4e536668878c28`.
- Multiplayer: `5524653d6a3ebf7027c4d342dfd88e99bae5e46e`.

## التحقق من الحزمة

- ZIP integrity: PASS.
- Manifest file sizes/SHA-256: PASS لجميع الملفات السبعة.
- Phase 76 static input tests: 5/5 PASS.
- Phase 77 PS3MAPI script tests: 5/5 PASS.

## ما لم يحدث

لم يتم تشغيل Pad Probe على PS3، ولم يتم التقاط ملفات SEG أو سجلات حقيقية. الحزمة وسيلة التقاط فقط؛ لا تنتج Local Client ثانياً ولا تعديلاً قابلاً للعب. الخطوة التالية بعد توفر الملفات الستة هي تشغيل `analyze_tu116_runtime_capture.py` ثم مراجعة Ghidra PPU XREFs.
