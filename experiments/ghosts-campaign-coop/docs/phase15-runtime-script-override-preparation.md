# Phase 15 — Runtime Script Override Preparation

## 1. لماذا انتقلنا إلى Runtime Injection؟

ملفات FastFile الخاصة باللعبة تحتوي على Auth Header وتوقيع RSA-2048. إعادة ضغط الكتل لا تنتج ملفاً صالحاً للتشغيل؛ إعادة بناء ملف موقع تتطلب مفتاح التوقيع الخاص غير المتاح. لذلك لا نعتمد على إعادة بناء `clockwork.ff` كمسار التسليم.

المسار الجديد هو تجهيز ScriptFile بديل داخل ذاكرة اللعبة، ثم اعتراض طلب تحميل الأصل وإرجاع ScriptFile البديل. نقطة البحث الأولية هي مسار البحث عن XAsset/ScriptFile، مثل `DB_FindXAssetHeader`. توجد مراجع IW6 على PC تستخدم اعتراض lookup لتقديم custom ScriptFiles، لكن عناوينها وبنية الذاكرة والـABI لا تُنقل إلى PS3 ولا تُستخدم كعناوين جاهزة.

## 2. ما أُضيف إلى PRX

أُضيف:

- `client/ps3/prx/script_override.c`
- `client/ps3/prx/script_override.h`

عند تحميل PRX يحاول قراءة الملف:

`/dev_hdd0/tmp/ghosts/maps__clockwork_interior.gscbin`

ثم يتحقق من:

- Magic: `GSC\0`.
- أطوال `compressedLen` و`len` و`bytecodeLen` بصيغة little-endian.
- أن مجموع أطوال الـwrapper يساوي حجم الملف بالضبط.
- أن الملف لا يتجاوز مساحة التخزين الثابتة 64 KiB.

لا يفك PRX ضغط الـstack ولا يكتب إلى ذاكرة اللعبة. يحتفظ بالـGSCBIN في مساحة PRX ثابتة ومحاذاة 16-byte، ويصدر NIDs تشخيصية لعرض الحالة والأطوال والعناوين.

### NIDs التشخيصية

| الوظيفة | NID |
|---|---|
| `ghc_script_override_reload` | `0xc0565cec` |
| `ghc_script_override_status` | `0xa671ac4f` |
| `ghc_script_override_comp_len` | `0x81c9916e` |
| `ghc_script_override_stack_len` | `0x59a70f67` |
| `ghc_script_override_bytecode_len` | `0xaec22a12` |
| `ghc_script_override_total_len` | `0x94e3b8bd` |
| `ghc_script_override_hash32` | `0x6302c47c` |
| `ghc_script_override_blob_address` | `0x8b2e694d` |
| `ghc_script_override_stack_address` | `0x44904b60` |
| `ghc_script_override_bytecode_address` | `0x7403e8b1` |

حالات `ghc_script_override_status`:

| القيمة | الحالة |
|---:|---|
| 0 | EMPTY |
| 1 | READY |
| 2 | OPEN_FAILED |
| 3 | READ_FAILED |
| 4 | BAD_MAGIC |
| 5 | BAD_LENGTHS |
| 6 | TOO_LARGE |

هذه الواجهة لا تعني أن Hook موجود؛ إنها تثبت فقط أن PRX يستطيع تحميل الـGSCBIN والتحقق من غلافه وإتاحة عنوانه.

## 3. أداة تحديد ScriptFile

أُضيفت:

`tools/ps3mapi/scan_clockwork_script_asset.py`

تستطيع الأداة:

- البحث عن `maps/clockwork_interior` داخل raw zone أو memory dump.
- التحقق من 24-byte ScriptFile prefix والـsentinels.
- فك ضغط zlib للـstack للتحقق من الحجم.
- حساب عنوان افتراضي إذا أُعطي base address الصحيح للـmemory dump.
- تحويل GSCBIN إلى ScriptFile asset blob بصيغة FastFile.
- حساب فرق الحجم بين الأصل والبديل دون تعديل أي ملف لعبة.

### نتيجة الحجم الحالية

| العنصر | الحجم |
|---|---:|
| ScriptFile الأصلي | 21,269 bytes |
| ScriptFile البديل | 22,927 bytes |
| الفرق | +1,658 bytes |

لذلك لا يجوز الكتابة فوق مساحة الأصل مباشرة أو قصّ الملف الجديد. يلزم فهم سجل ScriptFile داخل المحرك وتحديد طريقة نقل/تخصيص ذاكرة آمنة قبل إرجاع المؤشر إلى الـengine.

## 4. ما لم يتم تنفيذه

- لا يوجد Hook على `DB_FindXAssetHeader` حتى الآن.
- لم يتم تثبيت عنوان دالة أو prologue من PPU TU 1.16.
- لم يتم اعتماد حجم/ترتيب بنية ScriptFile في ذاكرة PS3؛ لا نفترض أن بنية PC IW6x تطابق PS3.
- لا يوجد تخصيص ذاكرة داخل game heap ولا تعديل لمؤشر asset registry.
- لا يوجد write إلى ذاكرة اللعبة، ولا تجاوز للتحقق من التوقيع.
- لم يتم اختبار PRX على جهاز PS3 بعد إضافة loader.

## 5. الاختبار التالي على الجهاز

1. ضع `maps__clockwork_interior.gscbin` في `/dev_hdd0/tmp/ghosts/`.
2. حمّل PRX المبني.
3. استدعِ `ghc_script_override_reload` ثم اقرأ `ghc_script_override_status`.
4. يجب أن تكون الحالة `1 (READY)`، والأطوال المتوقعة هي:
   - compressedLen = 6003
   - len = 24687
   - bytecodeLen = 16876
   - total wrapper bytes = 22895
5. سجّل عناوين blob وstack وbytecode من NIDs، دون الكتابة إليها.
6. احصل على runtime PPU dump المطابق لـTU 1.16 لتحديد مسار ScriptFile lookup وبنية XAssetHeader/ScriptFile الفعلية.

لا يتم تفعيل أي Hook قبل مطابقة عنوان الدالة وبايتات prologue مع تفريغ TU 1.16، وإضافة فحص يمنع الكتابة إذا اختلفت البايتات المتوقعة.
