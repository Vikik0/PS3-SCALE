# تقرير المرحلة 60 — حصر جميع مسارات حاويات اللاعبين الموجودة

## النطاق

تدقيق الـ68 دالة عالية الاستخدام لا يغطي كل الدوال التي تستعمل مؤشرات بديلة للاعب. لذلك أصبحت الأداة تفحص جميع الدوال في ملفات GSC الثلاثة بحثاً عن:

- level.playerpiggyback
- level.players
- level.playengineeffects

وتسجل الملف والدالة ونطاق الأسطر وعدد المراجع وتصنيف الملكية.

## النتائج على ملفات الاستخراج المرفوعة

| المهمة | الدوال التي تحتوي المؤشرات | playerpiggyback | playengineeffects |
|---|---:|---:|---:|
| Black Ice | 0 | 0 | 0 |
| Carrier | 4 | 9 | 4 |
| Cornered | 9 | 10 | 0 |
| **المجموع** | **13** | **19** | **4** |

لم يظهر أي مرجع إلى level.players في ملفات المهمات الثلاث.

## أهم المواقع

- Carrier: monitorscavengerpickup وgetenemytarget وplayerview_playmissanim وget_different_favoriteenemy.
- Cornered: getenemytarget وenable_stealth_system وdisable_stealth_system وevent_awareness_waitclear_ai_proc وsystem_message_loop وenemy_corpse_alert_level وenemy_threat_logic_dog_wait وcourtyard_rorke وbar_enemies.

## التعديلات

1. أصبحت أداة التدقيق تعرض جميع الدوال التي تحتوي مؤشرات playerpiggyback أو players أو playengineeffects، بالإضافة إلى قائمة الـ68 دالة عالية الاستخدام.
2. أضيف ملخص أعداد لكل مؤشر ولكل مهمة.
3. أضيف اختبار اصطناعي يثبت اكتشاف هذه المسارات حتى لو لم تكن الدالة ضمن قائمة الأهداف الرئيسية.

## الحدود

هذه مؤشرات في السكربتات فقط. لا تثبت أن playerpiggyback أو playengineeffects يمثلان Controller 2 أو أن Campaign ينشئ لاعباً محلياً ثانياً.
