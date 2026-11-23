# تقرير المرحلة 33 — مصفوفة ملكية حالة اللاعب في مهمات Campaign

## الهدف

بعد بناء Call Graph في المرحلة 32، أضفت تدقيقاً يميز عمليات اللاعب التي يجب أن تُنفذ على كيان اللاعب نفسه عن حقول المستوى وموارد المهمة التي قد تكون مشتركة أو تحتاج إلى مراجعة ملكيتها قبل التعديل.

الأداة لا تغيّر السكربتات، ولا تستبدل level.player تلقائياً. هدفها منع نقل حالة عالمية إلى لاعب واحد أو تكرار حدث المهمة مرتين عند إدخال لاعب ثانٍ.

## نطاق التحليل

تم فحص 17 دالة ذات أولوية من ملفات GSC المستخرجة في المهمات الثلاث:

- Black Ice: أربع دوال.
- Carrier: ست دوال.
- Cornered: سبع دوال.

تستخرج الأداة:
- عدد مراجع level.player.
- استدعاءات API الخاصة باللاعب وتصنيفها إلى إدخال، أسلحة، حركة وتحكم، كاميرا، HUD، صحة/حياة، ومؤثرات.
- حقول اللاعب المباشرة.
- استخدام level.playerpiggyback وlevel.players كقوائم لاعبين.
- استخدام level.playengineeffects كمرجع ثانوي منفصل عن قوائم اللاعبين.
- حقول level وواجهات flags وdvars التي تحتاج إلى مراجعة نطاقها.

## نتائج ذات أولوية

### Cornered

| الدالة | مراجع اللاعب | أبرز عمليات اللاعب | إشارات مستوى/مهمة مهمة |
|---|---:|---|---|
| gun_down_trigger_internal | 78 | 31 عملية سلاح | has_hidden_weapon_equipped وflags وdvar |
| fall_player | 47 | حركة 16، كاميرا 9، مؤثرات 8 | fall_arms_and_legs وfall_path_rig وfall_anim_struct وflags |
| upload_virus_enter | 44 | أسلحة 12، حركة 8 | device وvirus_dynamic_dof وallow_fire وflags وdvars |
| courtyard_rorke | 6 | صحة/حياة 1 | allies وoffice_guy_b وflags؛ ويستخدم playerpiggyback |
| bar_enemies | 3 | API لاعب 1 | allies وbar/stealth flags؛ ويستخدم playerpiggyback |
| enemy_threat_logic_dog_wait | 0 | لا توجد عملية مباشرة على level.player | playerpiggyback يستخدم لتوزيع threads على اللاعبين |
| enemy_corpse_alert_level | 0 | لا توجد عملية مباشرة على level.player | playerpiggyback لاختيار هدف العدو |

القرار: fall_player لا يمكن نسخه كما هو للاعب الثاني؛ لأنه يجمع حركة اللاعب وكاميرته مع rigs وأعلام تسلسل المهمة. يجب فصل rig اللاعب عن حالة الحدث المشترك. وكذلك gun_down_trigger_internal يجب أن يحفظ/يستعيد السلاح والذخيرة لكل لاعب، بينما يبقى علم تفعيل الحدث مرة واحدة على مستوى المهمة.

### Carrier

| الدالة | مراجع اللاعب | أبرز عمليات اللاعب | إشارات مهمة |
|---|---:|---|---|
| knockback_hesh | 51 | حركة 24، أسلحة 13، صحة 3، كاميرا 2 | slide_across_car_human وflags |
| cut_the_rope | 22 | حركة 8، أسلحة 6، صحة 2 | scoring_mode وflags |
| victory_choppers_land | 20 | حركة 10، أسلحة 9، صحة 1 | victory/autosave flags |
| playerview_playmissanim | 3 | لا يوجد استدعاء API مباشر عبر level.player | 3 مراجع إلى level.playengineeffects، وهو مرجع العميل/اللاعب الثاني حسب تحليل common loader السابق |
| monitorscavengerpickup | 0 | لا يوجد | يستخدم playerpiggyback في جسمه، لكنه بلا call site داخلي في ملفات Carrier المستخرجة |
| dog_monitor_goal_ent | 0 | لا يوجد | لا توجد عملية لاعب مصنفة ولا call site داخلي |

القرار: مشاهد Hesh والقطع والانتصار تحتاج إلى فصل التحكم والحركة والأنيميشن لكل لاعب عن حالة المشهد المشتركة. أما monitorscavengerpickup فلا يُعتمد عليه كأساس Co-op قبل إثبات استدعائه من سكربت مشترك أو من المحرك.

### Black Ice

| الدالة | مراجع اللاعب | أبرز عمليات اللاعب | إشارات مستوى/مهمة |
|---|---:|---|---|
| player_fail_finale_instakill | 26 | حركة 8، مؤثرات 6، كاميرا 2 | jump_type وflags وdvars |
| ascend_logic | 22 | حركة 4، كاميرا 2، صحة 1 | ascend/arms rigs وallies وflags |
| event_player_turns_key | 17 | حركة 8، أسلحة 8 | flarestack state |
| player_saw_kill | 1 | لا توجد مجموعة API كبيرة | قيد Split Screen على اللاعب الأساسي موثق في المرحلة 32 |

القرار: مشاهد النهاية والصعود يجب أن تملك سياق لاعب منفصل للكاميرا والـrig والحركة، مع عدم تكرار انتقال المهمة أو flags.

## الربط مع common loader في PS3

وفق التحليل السابق لسكربتات common الخاصة بـPS3 TU 1.16:

- init_level_flags() يبني level.playerpiggyback من كيانات player الموجودة عبر getentarray.
- level.player يشير إلى العنصر الأول.
- إذا وجد عنصر ثانٍ، يُحفظ مرجعه في level.playengineeffects.
- هذا initializer يجمع كيانات موجودة مسبقاً؛ لا ينشئ Local Client ثانياً ولا يربط Controller 2.

لذلك:
- playerpiggyback هو قائمة كيانات اللاعبين الموجودة.
- playengineeffects مرجع ثانٍ منفصل، وليس اسماً آخر لقائمة اللاعبين.
- ظهور هذه الحقول في سكربتات المهمة لا يثبت أن Campaign أنشأ لاعباً ثانياً في الجلسة.
- وجود _coop.gsc وHUD للاعب الآخر لا يثبت أن initializer الخاص به يُستدعى في Campaign.

## قواعد التعديل التي تعتمدها المرحلة

1. عمليات السلاح والذخيرة والإدخال والحركة والكاميرا والـHUD يجب أن تستقبل كيان اللاعب المستهدف صراحةً.
2. أعلام المهمة، checkpoint، autosave، rigs المشتركة، الأجهزة، وتقدم القصة لا تُنسخ تلقائياً لكل لاعب.
3. التفاعلات المشتركة يجب أن تستخدم قفلاً/حالة تمنع اللاعبَين من تفعيل الحدث نفسه مرتين.
4. قوائم playerpiggyback تُستخدم حيث توجد فعلاً، لكن لا نخلطها مع مرجع اللاعب الثاني playengineeffects.
5. لا يتم تعديل GSC تعاوني قبل إثبات native local-client الثاني وربط Controller 2.

## الاختبارات

أضيفت خمسة اختبارات اصطناعية تتحقق من فصل عمليات اللاعب عن حقول level، والتعرف على playerpiggyback، وتمييز playengineeffects عن قائمة اللاعبين، وتجاهل التعليقات والنصوص، والإبلاغ عن الدوال غير الموجودة دون تخمين.

هذه مرحلة تحليل ثابت؛ لم يتم تعديل ملفات اللعبة أو تشغيلها على PS3.
