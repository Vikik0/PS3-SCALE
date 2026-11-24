# تقرير المرحلة 87 — بناء حزمة التطوير تلقائياً من CI

## الهدف

نقلت تجهيز حزمة PS3 من خطوات يدوية إلى Job مستقل داخل GitHub Actions، بحيث تنتج كل دورة CI ناجحة حزمة موحدة من ملفات البناء والاختبارات والمراجع.

## ما أُضيف

- أداة tools/build_capture_kit.py تجمع ملفات PRX من خمسة CI artifacts:
  - Campaign Pad Probe v4.
  - DualShock Input Probe v1.
  - PS3 CEX bootstrap.
  - Campaign Local Session GSC bridge.
  - Campaign Player Registry GSC.
- تجمع أدوات PS3MAPI وملفات dump الخاصة بـCampaign وMultiplayer، وملفات mode selector، وGSC sources/manifests، وتقارير المراحل 84–86.
- تنقل GSCBIN إلى Experimental_GSC_Not_Installed حصراً، وتمنع وضعها ضمن ملفات PS3.
- تنشئ MANIFEST.json مع أحجام وSHA-256 لكل ملف.
- تشغّل verify_capture_kit.py على المجلد قبل ضغطه؛ أي ملف ناقص أو hash غير مطابق يوقف بناء الحزمة.
- أضيفت اختبارات builder للتحقق من نجاح البناء ومن رفض artifact ناقص.

## CI النهائي

GitHub Actions run [#1511](https://github.com/Vikik0/PS3-SCALE/actions/runs/45510341497):

- **7/7 jobs successful**، بما فيها package-capture-kit.
- **489 اختباراً** ناجحاً.
- حزمة CI artifact: `ghosts-tu116-development-capture-kit`.
- Artifact ID: `14974903362`.
- الملف الداخلي: `Ghosts_TU116_Development_Capture_Kit.zip`.
- الحجم: **483,396 bytes**.
- الملفات المدرجة: **46**.
- مدقق الحزمة تحقق من **46/46 ملفاً** ومن ملفي GSC manifests.
- Source commit: `2018f977415f3fa2afc108151184ce070e430c8e`.
- SHA-256 للحزمة: `477f6b84992369b78972cff16f7b57ed4df3d8cf4b395ed1f934ce162f44ea35`.

## النتيجة العملية

أصبح بالإمكان تنزيل حزمة متطابقة مع artifacts التي نجحت في CI، بدلاً من تجميع نسخ PRX وGSC يدوياً. كل ملف داخل الحزمة له hash مسجل، ويمكن إعادة فحصه محلياً قبل استخدامه.

## الحدود

- نجاح بناء الحزمة لا يثبت أن PRX أو GSC يعملان على PS3 فعلي.
- GSC Local Session bridge ما زال غير محقون في retail FastFile.
- Event Lock لم يُربط بعد بدوال المهمات الأصلية.
- لا يوجد إثبات لـNative Local Client 1 أو Campaign Player 2 قابل للعب.
- ما زالت سجلات Input Probe وPPU runtime dumps الفعلية مطلوبة لفتح بوابة التنفيذ على الجهاز.
