# تقرير المرحلة 88 — خريطة ملكية أحداث المهمات وربطها بأرقام الأقفال

## الهدف

حددت أي أحداث المهمات تحتاج قفل لاعب فعلي، وأيها يجب أن يبقى منسقاً عالمياً واحداً. هذا يمنع استعمال Event Lock في المكان الخطأ، خصوصاً للأحداث التي تنفذ فشل المهمة أو تغيّر فيزياء العالم أو تحفظ التقدم.

## التوزيع

| Event ID | المهمة | الدالة | السياسة |
|---:|---|---|---|
| 0 | Black Ice | event_player_turns_key | مالك واحد لتفاعل الكونسول |
| 1 | Carrier | cut_the_rope | مالك واحد للحبل |
| 2 | Cornered | gun_down_trigger_internal | مالك واحد لإخفاء السلاح |
| 3 | Cornered | upload_virus_enter | مالك واحد لرفع الفيروس |

الأرقام 4–15 متاحة لأهداف مشتركة أخرى بعد تحليلها. يجب أن يستخدم كل trigger أو handler لنفس الهدف الرقم نفسه.

## أحداث لا تستخدم Player Event Lock

- Black Ice — player_fail_finale_instakill: الفشل النهائي وearthquakes وdvars حدث مهمة واحد.
- Black Ice — ascend_logic: منسق صعود واحد؛ فصل إدخال وكاميرا اللاعب لا يعني تكرار حبال أو حلفاء أو timeline.
- Carrier — knockback_hesh: مشهد Sparrow وslow motion وautosave مرة واحدة.
- Carrier — victory_choppers_land: الهبوط والفوز والحفظ مرة واحدة.
- Cornered — fall_player: انهيار العالم والجاذبية والانفجارات والفشل والحفظ مرة واحدة.

هذه الأحداث تستخدم mission coordinator، وقد تحتاج handlers محلية للكاميرا أو rig، لكنها لا تحجز Event ID لكل لاعب.

## أداة التحقق

المسار: tools/verify_mission_event_lock_map.py

تتحقق من:
- وجود الدوال التسع الحرجة بالضبط.
- عدم تكرار Event IDs، وبقاء الأرقام ضمن 0–15.
- ثبات التعيين 0–3 للتفاعلات الأربعة.
- أن كل shared interaction لها مالك أول من يحصل على القفل.
- أن mission coordinator لا يدّعي Player Lock.
- وجود دوال acquire/release/abort/owner/reset في Local Session GSC.
- بقاء خريطة التنفيذ بحالة PLANNED_NOT_WIRED_TO_ORIGINAL_MISSION_HANDLERS.

## حدود الخريطة

هذه خريطة تنفيذ وليست patch. لم تتغير ملفات Black Ice أو Carrier أو Cornered الأصلية، ولم يُحقن أي GSCBIN في FastFile. لا يمكن ربط هذه الأقفال باللعبة قبل إثبات Native Local Client 1 وتسجيل إدخال اليد الثانية فعلياً.

## الخطوة التالية

بعد استلام Campaign/Multiplayer input logs وPPU runtime dumps، نربط Event IDs 0–3 فقط بالتفاعلات المشتركة، ونبقي الأحداث العالمية الخمسة تحت منسق واحد.
