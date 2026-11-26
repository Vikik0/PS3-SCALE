# تقرير المرحلة 107 — ربط Player Pair Capture بملف Campaign SELF الدقيق

اكتشفت أن محلل TU 1.16 كان يحفظ SHA-256 لملف Player Pair JSON، لكنه لا يتحقق أن الالتقاط يخص نفس ملف Campaign `default.self` الذي يجري تحليله حالياً.

هذا قد يسمح بإرفاق Capture قديم من نسخة أخرى تحمل نفس Title ID ورقم التحديث، ثم عرضه بجانب تحليل SELF مختلف.

## ما تم تطويره

- إضافة `tools/ps3mapi/campaign_capture_binding.py`.
- ربط الأداة بمحلل `analyze_tu116_runtime_capture.py`.
- إضافة الحقل `campaign_self_sha256` إلى قالب الالتقاط.
- عند وجود الهاش، يقارن المحلل القيمة مع SHA-256 الفعلي لملف Campaign SELF المستخدم.
- اختلاف الهاش يوقف التحليل قبل بدء إعادة بناء ELF.
- عند التطابق تظهر الحالة `PASS_CAMPAIGN_SELF_SHA256_MATCH`.
- إذا لم يوجد الحقل تظهر `NOT_PROVIDED` بدلاً من افتراض التطابق.

## طريقة استخراج الهاش على Windows

```powershell
(Get-FileHash "D:\Ghosts\TU116\default.self" -Algorithm SHA256).Hash
```

يوضع الناتج في `campaign_self_sha256` داخل ملف Player Pair Capture.

## الاختبارات والنتائج

أضفت:
- 5 اختبارات لوحدة ربط الهاش.
- اختبارين لتكامل الربط داخل محلل TU 1.16.
- اختبار يثبت أن اختلاف الهاش يوقف التحليل قبل استدعاء محلل ملفات الذاكرة.

تشغيل GitHub Actions النهائي على commit `b5132173e0bda4217f0bb29c3a5c75f0bdd5056a` نجح:

- **7/7 وظائف CI ناجحة**.
- **577/577 اختبار Python ناجح**.
- GSC compile/decompile: ناجح.
- PS3 PRX bootstrap: ناجح.
- Protocol/header وWorker وcapture metadata intake: ناجحة.
- Capture Kit: **57/57 ملفاً تم التحقق منها**.
- حجم الحزمة الداخلية: **538,015 bytes**.
- SHA-256: `55060f3c5b01778c8395d6f78c9b77b7ab85b3ffcd8b19f9c8f4cea91055ff2b`.
- Artifact ID: `15098789457`.

CI: https://github.com/Vikik0/PS3-SCALE/actions/runs/45783943444
Capture Kit: https://github.com/Vikik0/PS3-SCALE/actions/runs/45783943444/artifacts/15098789457

## حدود هذا التحقق

مطابقة SHA-256 تربط ملف JSON بملف SELF الذي يحلله البرنامج، لكنها لا تثبت مصدر الالتقاط ولا تثبت أن البيانات جُمِعت من جهاز PS3 حقيقي. لذلك بقيت حالة Player 2 على `NOT_PROVEN`، وبقي `capture_source_authenticated=false`.

لا يوجد Runtime capture حقيقي بعد، ولم يتم تعديل ملفات المهمات الأصلية أو FastFiles أو EBOOT. حزمة PRX الناتجة تشخيصية/bootstrap وليست نسخة Co-op قابلة للعب أو للتثبيت.
