# تقرير المرحلة 89 — تتبّع وصول اللاعب الفعلي إلى أحداث المهمات

## الهدف
قبل إضافة استدعاءات Event Lock إلى السكربتات الأصلية، تحققت من مسار وصول كيان اللاعب الذي فعّل التفاعل، وليس فقط من وجود الدالة أو رقم القفل.

## مصدر التحليل والتحقق
- الأرشيف: `Ghosts_Campaign_Missions_Phase31.zip`.
- SHA-256 للأرشيف: `ca4c0fb46a12b6179b406bc6f7ee5e10729c66ac7a7d2664bf9c3d3cfa14a77a`.
- تم تثبيت SHA-256 لخمسة ملفات GSC مرتبطة بمسارات التفاعل والمشغلات.
- أداة التدقيق: `tools/mission_event_activator_audit.py`.
- التقرير القابل للقراءة آلياً: `reports/PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json`.

## النتيجة

**الحالة: التوصيل الآمن محجوب حالياً**

| الحدث | Event ID | دليل المشكلة |
|---|---:|---|
| Black Ice — event_player_turns_key | 0 | لا يستقبل لاعباً؛ helper يعتمد على `level.player` ولا يعيد activator |
| Carrier — cut_the_rope | 1 | `var_0` هو map struct/animation target، و`self` هو كيان الحبل؛ ولا يوجد وسيط للاعب الفعلي |
| Cornered — gun_down_trigger_internal | 2 | يبدأ من global flag، ولا يستقبل activator، ويحتوي 78 مرجعاً إلى `level.player` |
| Cornered — upload_virus_enter | 3 | مسار الرفع يمر عبر flags عامة، ولا يمرر اللاعب بين دوال الدخول والخروج |

- التفاعلات المشتركة التي تم تدقيقها: **4**.
- تفاعلات جاهزة لربط القفل: **0**.
- تفاعلات محجوبة بسبب غياب مسار activator: **4**.
- أحداث المهمة التي يجب أن تبقى single-shot: **5**.

## أحداث لا نضع لها Player Event Lock

| المهمة | الحدث | السياسة |
|---|---|---|
| Black Ice | player_fail_finale_instakill | منسق فشل واحد |
| Black Ice | ascend_logic | منسق صعود واحد |
| Carrier | knockback_hesh | تسلسل Sparrow وslow motion مرة واحدة |
| Carrier | victory_choppers_land | فوز وحفظ مرة واحدة |
| Cornered | fall_player | انهيار العالم والجاذبية والفشل مرة واحدة |

## قرار التنفيذ

لم أضف أقفالاً إلى السكربتات الأصلية؛ لأن وضع القفل حول `level.player` سيحجز اللاعب الأساسي فقط، ولن يثبت ملكية اللاعب الثاني. يجب أولاً تمرير اللاعب الحقيقي من trigger إلى كل handler، مع فصل `self` (كيان الحبل) عن `var_0` (map struct/animation target) وعن وسيط اللاعب الجديد في Carrier، وفصل الحالة الخاصة بكل لاعب عن flags وdvars والأجهزة المشتركة.

## الخطوة التالية

1. إثبات وجود Native Local Client 1 وقراءة إدخال اليد الثانية على TU 1.16.
2. إعادة تصميم trigger helpers بحيث تلتقط activator حقيقياً.
3. تمرير اللاعب صراحةً عبر سلسلة كل تفاعل.
4. عندها فقط نضيف acquire/release/abort بأرقام 0–3.
5. إبقاء أحداث الفشل والصعود والانهيار والفوز تحت منسق مهمة واحد.

## حدود المرحلة

هذه مراجعة static للمصادر المفكوكة. لم تتغير ملفات المهمات الأصلية، ولم يُبنَ أو يُحقن FastFile، ولم يُجرَ اختبار PS3 فعلي.

## نتائج CI وحزمة التسليم

- GitHub Actions run [#1535](https://github.com/Vikik0/PS3-SCALE/actions/runs/45590863539): **7/7 jobs successful**.
- **508 اختباراً** ناجحاً.
- تم بناء الحزمة تلقائياً والتحقق منها قبل رفعها.
- Artifact ID: `15011056497`.
- حجم الحزمة الداخلية: **523,270 bytes**.
- التحقق: **55/55 ملفاً**، وملفا GSC manifest، وتقرير Activator Audit.
- SHA-256 للحزمة الداخلية: `853c3958d9601b7e63b6d7ef5d0fb2e42b6c5fb6f4cb878792d70c5fc655a702`.
- رابط الـartifact: https://github.com/Vikik0/PS3-SCALE/actions/runs/45590863539/artifacts/15011056497
