# Phase 13 — Clockwork drill source audit

## الهدف

هذه الأداة تفحص المصدر الأصلي والمصدر المعدّل لدورة الحفر في مهمة Clockwork. تركيزها على التقاط اللاعب مباشرةً من حدث الـTrigger، وتمريره إلى منطق الحفر، ومنع رجوع الدوال المساندة إلى `level.player` بصورة غير مقصودة.

الأداة:

`tools/clockwork_drill_adapter_audit.py`

الأداة للقراءة فقط؛ لا تعدّل المصدر ولا تترجم GSC ولا تعيد بناء FastFile.

## مصدر المرجع

- FastFile: `clockwork.ff`
- SHA-256 للـFastFile: `a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783`
- SHA-256 للمصدر الأصلي `maps__clockwork_interior.gsc`: `7fc0f02476d5c9ebbe7c28e68abe762a26a976a28587228c5027d1a5c543e1f1`

تُستخدم الدوال الست التالية لمطابقة المصدر الأصلي مع مصفوفة التحليل:

| الدالة | نطاق الأسطر الأصلي | مراجع `level.player` |
|---|---:|---:|
| `switchmsg` | 263–277 | 8 |
| `handle_drill_spot` | 278–503 | 32 |
| `restore_footstep_notetrack_scripts` | 504–514 | 4 |
| `handle_drilling` | 563–707 | 10 |
| `drillthrough_plate_sound` | 717–734 | 1 |
| `handle_misc_drill_details` | 760–848 | 15 |
| **المجموع** | | **70** |

`switchmsg` مستثناة من تدقيق ملكية اللاعب الثاني لأنها تنفذ التقاط أداة الحفر السينمائي الخاص بـLogan. يجب أن تبقى دالة اللاعب الأساسي، مع منح `drill_press` للاعب الثاني بصورة منفصلة.

## التشغيل على Windows

شغّل من مجلد:

`experiments/ghosts-campaign-coop`

لفحص المصدر المعدّل مع مقارنة الأصل:

```powershell
python tools/clockwork_drill_adapter_audit.py "D:\Ghosts\patched\maps__clockwork_interior.gsc" --reference-source "D:\Ghosts\original\maps__clockwork_interior.gsc" --json-out "D:\Ghosts\analysis\clockwork_drill_source_audit.json" --strict
```

## ما الذي يفحصه؟

- وجود الدوال الأساسية: `handle_drill_spot` و`handle_drilling` و`handle_misc_drill_details`.
- وجود الدوال المساندة التي يجب أن تستقبل اللاعب صراحةً: `watch_for_air_drop_death` و`restore_footstep_notetrack_scripts` و`drillthrough_plate_sound` و`stop_current_car_watcher`.
- التقاط اللاعب مباشرةً من `drill_trigger waittill( "trigger", drill_activator )` داخل `handle_drill_spot`، ثم ضبط Flag المهمة مع تمرير اللاعب نفسه.
- عدم الاعتماد على `flag_wait` لالتقاط اللاعب؛ بعض callbacks في Split-Screen قد تنتظر لمس اللاعبين الاثنين ثم تضبط الـFlag دون تمرير الـactivator.
- تمرير `drill_player` إلى `handle_drilling`، ومنع عامل الحفر من التعليق داخل `flag_wait` إذا مات اللاعب وتمت إعادة ضبط الـFlag.
- التحقق من أن اللاعب الثاني هو الـnative secondary player عبر `issplitscreenplayer()` و`issplitscreenplayerprimary()`.
- تجهيز loadout اللاعب الثاني بأسلحة Clockwork والـGrenades.
- تشغيل مساعدات الحفر لكل لاعب على حدة، وإدارة `ammoCounterHide` على مستوى اللاعبين الاثنين.
- فصل حالة `aiming_at_ally` وحفظ ذخيرة الـGrenades لكل لاعب.
- تنظيف حالة الحفر عند الموت أو الانقطاع، وإعادة ضبط Flag البداية، وانتظار انتهاء دورة تنظيف قصيرة قبل إعادة تفعيل الـTrigger.
- حفظ قيمة `ammoCounterHide` الأصلية وإعادتها بدلاً من فرض القيمة `0` بعد انتهاء الحفر.
- إبقاء `switchmsg` دالة Logan الأساسية مع منح اللاعب الثاني `drill_press` وإزالته بعد نهاية جزء الخزنة.

يسمح الفحص بخمسة استخدامات محددة لـ`level.player` داخل `handle_drill_spot` فقط عندما تكون fallback أو مقارنة هوية مع اللاعب الأساسي؛ لا يسمح باستخدامها لاستدعاء methods الخاصة باللاعب داخل مسار الحفر.

## تفسير النتيجة

| الحالة | معناها |
|---|---|
| `SOURCE_INCOMPLETE` | إحدى الدوال المطلوبة غير موجودة. |
| `SOURCE_REFERENCE_MISSING` | شُغّل الوضع الصارم دون تمرير المصدر الأصلي. |
| `SOURCE_BASELINE_MISMATCH` | المصدر المرجعي لا يطابق SHA-256 ومصفوفة Clockwork الأصلية. |
| `BLOCKED_INCOMPLETE_PLAYER_SCOPING` | مسار التقاط اللاعب أو تمريره أو تنظيفه غير مكتمل، أو توجد مراجع غير مسموحة إلى اللاعب الأساسي. |
| `SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW` | اجتاز المصدر الفحوص الثابتة؛ ما زال يحتاج مراجعة واختباراً داخل اللعبة. |

## حدود الفحص

- الفحص ثابت ولا يثبت أن اللعبة أنشأت Local Client ثانياً.
- التقاط activator يستخدم حدث `trigger` القياسي مباشرةً، بدلاً من الاعتماد على callback الذي يضبط Flag المهمة. يجب تأكيد أن حدث Trigger الخاص بنقطتي الحفر يصل إلى المستمع المباشر على نسخة اللعبة المستهدفة.
- إذا لم يمرر الـTrigger اللاعب، يستخدم التعديل fallback إلى Logan حتى لا يكسر اللعب الفردي.
- لا يثبت الفحص استقلال الكاميرا أو HUD.
- لا يعيد بناء `clockwork.ff` ولا ينشئ PKG.

## الاختبارات

تغطي الاختبارات تمرير اللاعب، كشف المراجع غير المباشرة، فصل الدوال الأساسية عن `switchmsg`، التحقق من الـnative secondary identity، حماية مسارات الموت والانقطاع، مطابقة المصدر المرجعي، ورفض الملفات غير التابعة لـGSC.
