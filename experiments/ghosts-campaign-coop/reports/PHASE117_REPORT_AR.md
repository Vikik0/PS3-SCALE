# تقرير المرحلة 117 — دعم أحداث أزرار اللاعب مع Event Lock

## ما تمت إضافته

أضفت دالة جديدة إلى Campaign Local Session Bridge:

`ghc_campaign_wait_for_owned_player_command(player, command, event_id)`

هذه الدالة تدعم التفاعلات التي تستخدم `notifyonplayercommand` بدلاً من حدث Trigger الخاص بكيان في العالم.

مسارها:
1. تستقبل كيان لاعب محدداً واسم الحدث ورقم Event ID.
2. تتحقق من جاهزية الجلسة وصحة Native Player Pair.
3. تتأكد أن اللاعب أحد الكيانين المحليين المسجلين.
4. تنتظر حدث الأمر على كيان اللاعب نفسه.
5. تعيد التحقق من الجلسة واللاعب بعد الانتظار.
6. تحجز Event Lock للاعب نفسه ثم تعيد كيان اللاعب.

على المهمة تسجيل `notifyonplayercommand` قبل استدعاء الدالة، ثم استخدام دوال Release أو Abort الموجودة بعد انتهاء التفاعل.

## سبب الإضافة

تفاعل Carrier لقطع الحبل يستخدم:
- `notifyonplayercommand("cut", "+melee")`
- `notifyonplayercommand("cut", "+melee_breath")`
- `notifyonplayercommand("cut", "+melee_zoom")`

لذلك لا يكفي انتظار حدث `trigger` على كيان في العالم. أصبح لدينا الآن Adapter منفصل لكل نوع من الحدثين.

## نتائج CI

GitHub Actions [#1646](https://github.com/Vikik0/PS3-SCALE/actions/runs/45865170243):

- **7/7 وظائف ناجحة**.
- **607/607 اختبار Python ناجح**.
- ترجمة وفك ترجمة Campaign Local Session Bridge: ناجح.
- بناء PS3 PRX وفحوصات البروتوكول والـProbes: ناجح.
- حزمة الالتقاط: **57/57 ملفاً** تم التحقق منها.

بصمة المصدر:
`b2377f0cd7b8d1455e323555da28954b0d53a990a418efe408864956bfb247f6`

بصمة GSCBIN:
`85f7ddd1d91a1fa8e3a1d06bee3104d80c72937473433465b65b27153b6e04ed`

الحجم: **5,903 bytes**.

[حزمة التطوير والالتقاط](https://github.com/Vikik0/PS3-SCALE/actions/runs/45865170243/artifacts/15140641180).

## ما لم يتم بعد

لم يتم تعديل Handler قطع الحبل الأصلي أو أي Call Site داخل المهمات. الدالة الجديدة مجرد أساس آمن للترحيل القادم؛ إذ يجب فصل حالة كل لاعب عن الآخر مع إبقاء الحبل والأنيميشن والـflags المشتركة تحت منسق واحد.

لم يتم إثبات Native Player 2 داخل Campaign على PS3 فعلي. Player 2 ما زال **NOT_PROVEN**، والتفاعلات المشتركة **0/4**. لم يتم تعديل EBOOT أو SELF أو FastFile.
