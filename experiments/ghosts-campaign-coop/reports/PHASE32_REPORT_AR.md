# تقرير المرحلة 32 — تتبّع مسارات تنفيذ GSC في Black Ice وCarrier وCornered

## الهدف

المرحلة 31 أحصت مراجع level.player وحددت بعض الدوال التي تستخدم level.playerpiggyback، لكنها لم تكن كافية لمعرفة ما إذا كانت الدوال متعددة اللاعبين مرتبطة فعلياً بمسارات تشغيل المهمة. تضيف المرحلة 32 محلل Call Graph يربط تعريفات الدوال باستدعاءاتها المباشرة، واستدعاءات thread، والاستدعاءات ذات namespace، ومراجع function-pointer التي تسجل callbacks.

## نطاق التحليل

تم تشغيل الأداة على ملفات GSC المفكوكة من حزمة المرحلة 31:

| المهمة | ملفات GSC | تعريفات الدوال | مراجع داخلية |
|---|---:|---:|---:|
| Black Ice | 76 | 1,217 | 1,735 |
| Carrier | 118 | 1,537 | 2,303 |
| Cornered | 55 | 1,550 | 3,116 |
| **المجموع** | **249** | **4,304** | **7,154** |

تصنيف المراجع:
- 1,973 استدعاء مباشر.
- 3,058 استدعاء thread.
- 1,310 استدعاء namespaced.
- 813 مرجع function-pointer / callback.

الأداة تستخدم تعريفات magicreloadwhenreachenemy كجذور مرشحة للسكربتات، ثم تحسب الوصول المباشر منفصلاً عن الوصول الذي يمر عبر callback registration.

## النتائج المهمة

### Carrier

1. monitorscavengerpickup() في 1031.gsc لا يملك أي call site داخل ملفات Carrier المستخرجة. وجوده في الملف لا يكفي لاعتباره مساراً فعالاً.
2. dog_monitor_goal_ent() في maps/ss_util.gsc لا يملك call site داخلياً أيضاً. الدالة get_different_favoriteenemy() لها استدعاءان داخل السلسلة نفسها، لكن السلسلة لا تصل إلى جذور السكربتات المرشحة في هذا الجرد.
3. مسار هجوم الكلب مختلف: magicreloadwhenreachenemy في animscripts/dog/dog_combat.gsc يصل إلى meleeattackcheck_whilemoving، ثم يسجل handlemeleefinishattacknotetracks كـ callback، ومنها يصل إلى playerview_spawn ثم playerview_playmissanim. هذا مسار موصول بالسكربت عبر callback، ويستخدم showonclient وhideonclient عند التعامل مع اللاعب والدرون. لا ينشئ هذا المسار لاعباً محلياً ثانياً.
4. knockback_hesh وcut_the_rope وvictory_choppers_land لها مسارات استدعاء قابلة للوصول في الرسم، لكنها تحتوي عمليات حركة/أنيميشن/تحكم على level.player، ولذلك تحتاج إلى فصل حالة اللاعب عن أعلام المهمة المشتركة قبل أي تعديل تعاوني.

### Cornered

1. cornered_starts() يسجل بدايات المهمة باستخدام maps\\_utility::add_start ومراجع callback. من هذا المسار يصل begin_courtyard إلى courtyard_intro_handler ثم courtyard_rorke.
2. مسار منطقة البار يصل من setup_atrium_check إلى bar_prep ثم bar_enemies. الدالتان courtyard_rorke وbar_enemies تحتويان على تعامل مع قائمة level.playerpiggyback، لذا فهذه ليست مجرد دوال غير مستعملة في ملفات المهمة.
3. مسار ذكاء الكلاب قابل للتتبع: callback الخاص بتهيئة AI يمر عبر stealth_debug_print وstealth_plugin_smart_stance وstealth_threat_behavior_replace، ثم enemy_threat_loop وenemy_threat_logic_dog وenemy_threat_logic_dog_wait. الأخيرة تمرر level.playerpiggyback ومرجع enemy_threat_logic_dog_wakeup_dist إلى array_thread.
4. مسار تنبيه الجثث يصل إلى enemy_corpse_alert_level، التي تختار هدفاً من level.playerpiggyback عندما لا يكون لدى العدو هدف محدد.
5. gun_down_trigger_internal وfall_player وupload_virus_enter لها call sites قابلة للوصول، لكنها تنفذ عمليات على اللاعب الأساسي وتستخدم أعلاماً وموارد عالمية مثل rigs والأجهزة وحالة المهمة. لا يصح تكرارها آلياً للاعب الثاني.

### Black Ice

1. player_saw_kill() مستدعاة من player_swim_rubberband() في سكربت السباحة.
2. player_saw_kill() تشغّل player_scuba() في الوضع العادي، أما عند Split Screen فتقصر تشغيلها على self == level.player. هذا قيد صريح على اللاعب الأساسي في هذا المسار.
3. event_player_turns_key() وascend_logic() وplayer_fail_finale_instakill() لها مسارات استدعاء داخل السكربتات المستخرجة، وتعتمد على rig وحالة حركة/كاميرا/سلاح اللاعب الأساسي.

## تفسير النتائج وحدودها

- call site موجود يعني أن هناك مرجعاً نصياً داخلياً إلى الدالة؛ لا يعني أن الحدث سيقع في كل جلسة لعب.
- callback registration يظهر كحافة مستقلة؛ تسجيل callback لا يثبت وحده أن المحرك نفذه في جلسة معينة.
- بعض الأسماء namespaced لا تطابق اسم ملف GSC بعد الاستخراج الرقمي، فتم حل بعضها عبر اسم دالة فريد. هذه النتائج أقل ثقة من تطابق مسار الملف، والأداة تسجل طريقة الحل.
- الأسماء الملتبسة التي لها أكثر من تعريف لا يتم تخمين هدفها.
- الجرد محصور بملفات GSC الـ249 المستخرجة. لا يمكنه استبعاد callbacks من ملفات اللعبة المشتركة أو التسجيل الديناميكي غير الموجود في هذه الحزمة.
- لا يثبت هذا التحليل وجود local client ثانٍ في Campaign، ولا يثبت ربط Controller 2 أو كاميرا/HUD مستقلة.

## القرار الهندسي

- لا تستخدم monitorscavengerpickup أو dog_monitor_goal_ent كأساس للتعاون في Carrier قبل إثبات مسار استدعائهما من ملفات مشتركة أو من المحرك.
- استخدم مسارات Cornered التي تمر عبر level.playerpiggyback كأمثلة حقيقية على منطق AI يتعامل مع قائمة اللاعبين، مع الحفاظ على أعلام المهمة المشتركة.
- لا تستبدل level.player بشكل شامل. افصل عمليات الإدخال والسلاح والحركة والكاميرا والأنيميشن لكل لاعب عن flags وcheckpoint والـworld state المشتركة.
- لا أضيف تعديلات GSC تعاونية غير قابلة للاختبار قبل إثبات native second-client slot. المرحلة التالية هي تحويل نتائج call graph إلى مصفوفة ملكية حالة per-player/shared لكل وظيفة ذات أولوية، ثم مراجعة مسار إنشاء العميل في ملفات المحرك المتاحة.

## الاختبارات ونتيجة CI

أضيفت 6 اختبارات اصطناعية تغطي تجاهل التعليقات والنصوص، حدود الدوال، الاستدعاءات المباشرة وthread وnamespace، مراجع callbacks، عدم تخمين الأسماء الملتبسة، وعدم اعتبار الدوال غير المستدعاة قابلة للوصول.

نجح GitHub Actions run رقم 45250534837 في جميع الوظائف الخمس، ونجحت 290 اختبارات Python، إضافة إلى بناء PRX وفحوص GSC وWorker وProtocol.

لم يتم تشغيل اللعبة أو تعديل ملفات FastFile الأصلية في هذه المرحلة.
