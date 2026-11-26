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

## حدود هذا التحقق

مطابقة SHA-256 تربط ملف JSON بملف SELF الذي يحلله البرنامج، لكنها لا تثبت مصدر الالتقاط ولا تثبت أن البيانات جُمعت من جهاز PS3 حقيقي. لذلك بقيت حالة Player 2 على `NOT_PROVEN`، وبقي `capture_source_authenticated=false`.

## الاختبارات

أضفت:
- 5 اختبارات لوحدة ربط الهاش.
- اختبارين لتكامل الربط داخل محلل TU 1.16.
- اختبار يثبت أن اختلاف الهاش يوقف التحليل قبل استدعاء محلل ملفات الذاكرة.

نتائج CI النهائية ستضاف بعد اكتمال تشغيل GitHub Actions.
