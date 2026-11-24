# تقرير المرحلة 86 — مدقق سلامة حزمة PS3

## الهدف

أضفت أداة مستقلة للتحقق من حزمة التطوير والالتقاط قبل نسخ أي ملفات إلى جهاز PS3. الهدف اكتشاف الملفات الناقصة أو المعدّلة، ومنع وضع GSCBIN التجريبية داخل مجلد ملفات PRX.

## الأداة

المسار: tools/verify_capture_kit.py

تتحقق الأداة من:
- وجود MANIFEST.json وصحة Title ID = BLUS31270 وTU = 01.16.
- وجود كل الملفات الأساسية: PRX probes، bootstrap، وملفات PS3MAPI الخاصة بـCampaign وMultiplayer.
- تطابق حجم وSHA-256 لكل ملف مُدرج في manifest.
- سلامة مسارات الملفات ومنع path traversal أو الخروج من مجلد الحزمة.
- تطابق SHA-256 وحجم GSCBIN مع manifests الخاصة بـLocal Session وPlayer Registry.
- وجود ملفات GSC التجريبية في Experimental_GSC_Not_Installed فقط، وعدم وجود GSCBIN داخل مجلد PS3.
- بقاء وصف الحزمة واضحاً بأنها ليست co-op قابلة للعب، وأن GSCBIN غير محقونة أو مختبرة على الجهاز.

## الاختبارات

أضيفت سبعة اختبارات تغطي:
1. حزمة سليمة تمر بجميع فحوصات hashes.
2. اكتشاف ملف تم العبث به.
3. اكتشاف غياب أحد probes.
4. رفض مسار manifest يحاول الخروج من جذر الحزمة.
5. رفض Title Update غير مطابق.
6. اكتشاف عدم تطابق مصدر GSC مع manifest.
7. رفض وضع GSCBIN داخل مجلد PS3.

## النتائج

- GitHub Actions run [#1499](https://github.com/Vikik0/PS3-SCALE/actions/runs/45509141329): **6/6 jobs successful**.
- **487 اختباراً** ناجحاً.
- شغّلت مدقق السلامة على الحزمة الفعلية بعد تجهيزها: **29/29 ملفاً تم التحقق منها**، وملفا GSC manifests صحيحان.
- حزمة التسليم: `Ghosts_TU116_Development_Capture_Kit_Phase85.zip`.
- الحجم: 429,668 bytes.
- SHA-256: `0e6ba4e8da8539a787f0cbbf2312c8054b73337687df6ba15014474bb09d22bd`.

## طريقة الاستخدام

بعد فك الضغط، شغّل من جذر الحزمة:

~~~powershell
python .\Tools\verify_capture_kit.py
~~~

النتيجة المطلوبة: PASS. إذا ظهر FAIL، لا تنسخ الملفات إلى الجهاز قبل معالجة السبب.

## الحدود

مدقق الحزمة يثبت سلامة الملفات ومطابقتها للـmanifest فقط. لا يثبت أن PRX أو GSC يعملان على جهاز PS3، ولا ينشئ Local Client، ولا يثبت Campaign co-op. ملفات GSC التجريبية تبقى خارج مجلد PS3 ولا تُحمّل كأنها PRX.
