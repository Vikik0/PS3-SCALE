# تقرير المرحلة 90 — خريطة اعتماديات تفاعلات المهمات

## الهدف
حوّلت نتائج المرحلة 89 إلى خريطة دقيقة لسلاسل الاستدعاء، حتى نعرف أين يجب تمرير كيان اللاعب وأي حالة يجب أن تبقى مشتركة ولا تتكرر.

## النتائج
- **4 سلاسل تفاعل** موزعة على Black Ice وCarrier وCornered.
- **29 دالة** موثقة مع مواقعها وعدد مراجع \`level.player\`.
- **31 وصلة استدعاء** مثبتة من ملفات GSC الأصلية المفكوكة.
- **5 أحداث مهمة** بقيت ضمن منسق واحد ولا تحصل على Player Event Lock.

## سلاسل الاعتماد

### 1. Black Ice — Event ID 0
\`magicreloadwhenreachenemy → event_player_turns_key → waittill_trigger\`

المعالج الحالي يعتمد على \`level.player\` في 17 موضعاً، والمساعد على 13 موضعاً. الحل ليس تغيير المساعد العام؛ بل إنشاء trigger helper منفصل يستقبل اللاعب الحقيقي، مع بقاء console وflare-stack flags تحت منسق واحد.

### 2. Carrier — Event ID 1
\`zodiac_osprey_wave → ranktable → shoot_rocket → run_corpse_cleanup → wait_for_capture / cut_the_rope\`

تم تثبيت سياق الكيانات:
- \`ranktable\` يعمل على AI rappeller.
- \`var_0\` هو map struct/animation target.
- \`self\` في \`shoot_rocket\` و\`run_corpse_cleanup\` و\`cut_the_rope\` هو rope animation entity.
- اللاعب ليس \`self\` ولا \`var_0\`؛ يجب تمريره بوسيط مستقل إلى watcher الخاص بكل لاعب.

التصميم الآمن: إنشاء input watcher لكل Local Player صالح، ثم أخذ Event Lock 1 فقط عندما يبدأ اللاعب فعلياً قص الحبل. يبقى rope animation وhint state مشتركين، ويجب أن يظهر التلميح إذا كان أي لاعب مؤهلاً.

### 3. Cornered — Weapon Hide — Event ID 2
\`upload_virus_loop_anims → gun_down_trigger → gun_down_trigger_internal → determine_weapon / hold_count_check\`

المعالج الداخلي يحتوي 78 مرجعاً إلى \`level.player\`. يجب فصل weapon snapshots وinput state لكل لاعب، مع إبقاء \`gun_down_trigger\` flags و\`ammoCounterHide\` و\`aim_aimAssistRangeScale\` تحت منسق واحد.

### 4. Cornered — Virus Upload — Event ID 3
\`upload_virus_loop_anims → upload_virus_anims → upload_progress → upload_virus_enter / upload_virus_exit\`

الـ upload يستخدم device وanimation arrays وDOF وshadow phone state وglobal flags. هذه الموارد مشتركة ويجب أن يكون لها owner واحد؛ أما camera/weapon/movement/input فتكون per-player.

## تحذير مهم حول helpers العامة

- \`black_ice_util::waittill_trigger\`: 3 call sites.
- \`player_scripted_anim_util::waittill_trigger\`: 15 call sites.

تغيير هذه الدوال مباشرة قد يكسر تفاعلات أخرى في Black Ice وCarrier وCornered. لذلك الخطة هي إضافة helper جديد player-aware ونقل call sites المستهدفة فقط.

## بوابة التنفيذ
لن أعدّل ملفات المهمات الأصلية قبل إثبات Native Local Client 1 على TU 1.16. بعد الإثبات:
1. إضافة player-aware trigger helpers منفصلة.
2. تمرير اللاعب عبر كل سلسلة player-owned.
3. إبقاء العالم والأجهزة والflags والـ dvars تحت منسق واحد.
4. ربط Event Lock IDs 0–3 عند بداية التفاعل الفعلية.
5. اختبار الإلغاء والموت وdisconnect لمنع بقاء lock محجوزاً.

## حدود المرحلة
هذه خريطة static dependency/call-site. لم تتغير ملفات GSC الأصلية، ولم يُبنَ أو يُحقن FastFile، ولا يوجد ادعاء باختبار PS3 فعلي.

## نتائج البناء والتحقق

- GitHub Actions run [#1554](https://github.com/Vikik0/PS3-SCALE/actions/runs/45593854111): **7/7 jobs successful**.
- **513 اختباراً** ناجحاً.
- تم بناء حزمة Capture Kit والتحقق منها آلياً.
- Artifact ID: `15011738053`.
- الحزمة الداخلية: **534,872 bytes**.
- التحقق: **57/57 ملفاً**؛ ملفا GSC manifests، تقرير activator audit وخريطة refactor graph.
- SHA-256 للحزمة الداخلية: `d854b621df9e4ae12a1e96cc3c46bce78928ae4c6d7c7c1eceed67f06b3857af`.
- رابط الـartifact: https://github.com/Vikik0/PS3-SCALE/actions/runs/45593854111/artifacts/15011738053
