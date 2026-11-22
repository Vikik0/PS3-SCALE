# Phase 13 — Clockwork drill source adapter audit

## الهدف

تجهيز فحص آلي قبل تعديل سكربت Clockwork الأصلي. جسر Phase 12 يوفّر دوال ملكية Drill، لكن لا يجوز اعتبارها مدمجة حتى نعرف أن دوال المهمة الأصلية تمرّر اللاعب الذي بدأ التفاعل، وأن كل دوال المساعدة تستخدم اللاعب نفسه بدلاً من `level.player`.

الأداة الجديدة للقراءة فقط:

`tools/clockwork_drill_adapter_audit.py`

لا تعدّل المصدر، ولا تترجم GSC، ولا تعيد بناء FastFile أو EBOOT.

## التشغيل على Windows

ضع ملف GSC المستخرج من نسختك بجانب ملفات التحليل، ثم شغّل من مجلد:

`experiments/ghosts-campaign-coop`

```powershell
python tools/clockwork_drill_adapter_audit.py "D:\Ghosts\decompiled\maps__clockwork_interior.gsc" --json-out "D:\Ghosts\analysis\clockwork_drill_source_audit.json"
```

لإيقاف الأمر برمز خطأ إذا لم يجتز المصدر بوابة الفحص:

```powershell
python tools/clockwork_drill_adapter_audit.py "D:\Ghosts\decompiled\maps__clockwork_interior.gsc" --strict
```

## ما الذي يفحصه؟

- وجود `handle_drill_spot` و`handle_drilling`.
- عدد مراجع `level.player` داخل الدالتين.
- دوال GSC المحلية التي تستدعيها كل دالة، ثم تتبع سلسلة الاستدعاءات المحلية حتى نهايتها.
- أي مرجع `level.player` مخفي داخل دالة مساعدة متفرعة من مسار Drill.
- تمرير اللاعب صراحةً إلى دوال المساعدة ورصد استدعاءات input وweapon وanimation وcontrol وHUD.
- وجود `ghc_campaign_drill_begin` في مدخل التفاعل، ووجود مساري complete وcancel ضمن سلسلة التفاعل.
- وجود استدعاء `ghc_campaign_drill_abort_player` في المصدر لتنظيف التفاعل عند الموت أو الانقطاع.

## تفسير النتيجة

| الحالة | معناها |
|---|---|
| `SOURCE_INCOMPLETE` | إحدى الدالتين الأصليتين غير موجودة في الملف المدخل. |
| `BLOCKED_INCOMPLETE_PLAYER_SCOPING` | ما زالت هناك مراجع للاعب الأساسي أو أن أحد مسارات begin/complete/cancel ناقص. |
| `SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW` | اجتاز المصدر الفحوص الثابتة الأولية، ويحتاج مراجعة يدوية لمسارات التنفيذ والتنظيف. |

حتى الحالة الأخيرة لا تعني أن اللعب يعمل. يجب التأكد يدوياً من أن اللاعب الممرر هو صاحب الضغط الفعلي، وأن رقم Drill spot صحيح، وأن السلاح وADS/fire والوقفة والأنيميشن وقيود الحركة وAction Slot كلها تخص اللاعب نفسه، وأن الإلغاء والموت والـCheckpoint تنظف الحالة دون إكمال الهدف.

## حدود هذه المرحلة

- لم نعدّل سكربتات اللعبة الأصلية لأن مصدر GSC الأصلي غير موجود في مستودع المشروع.
- لا نستخدم أرقام الأسطر القديمة من جدول التحليل كتعليمات Patch؛ يجب فحص المصدر المستخرج نفسه.
- لا نضيف Hook أو عنوان ذاكرة اعتماداً على أسماء الدوال أو نتائج هذا التقرير.
- إنشاء Local Client ثانٍ داخل Campaign وربط Controller 2 والكاميرا والـHUD ما زال يحتاج دليل Runtime من TU 1.16.
- لا يتم إنشاء PKG أو الادعاء بوجود نسخة قابلة للعب من هذه الأداة.

## الاختبارات

تغطي اختبارات Phase 13 المصدر الأصلي غير المعدّل، والمصدر المعدّل افتراضياً، والمراجع غير المباشرة داخل الدوال المساعدة، والتعليقات والنصوص، ونقص الدوال المطلوبة، ورفض الملفات غير التابعة لـGSC.
